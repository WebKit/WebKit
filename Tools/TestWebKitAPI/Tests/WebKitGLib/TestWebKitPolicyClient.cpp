/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include "LoadTrackingTest.h"
#include "WebKitTestServer.h"
#include "WebKitWebsitePolicies.h"
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

static WebKitTestServer* kServer;

class PolicyClientTest: public LoadTrackingTest {
public:
    MAKE_GLIB_TEST_FIXTURE(PolicyClientTest);

    enum PolicyDecisionResponse {
        Use,
        UseWithPolicy,
        Ignore,
        Download,
        None
    };

    static void testHandlerMessageReceivedCallback(WebKitUserContentManager* userContentManager, WebKitJavascriptResult* javascriptResult, PolicyClientTest* test)
    {
        GUniquePtr<char> valueString(WebViewTest::javascriptResultToCString(javascriptResult));
        if (g_str_equal(valueString.get(), "autoplayed"))
            test->m_autoplayed = true;
        else if (g_str_equal(valueString.get(), "did-not-play"))
            test->m_autoplayed = false;

        g_main_loop_quit(test->m_mainLoop);
    }

    static gboolean quitMainLoopLater(GMainLoop* loop)
    {
        g_main_loop_quit(loop);
        return FALSE;
    }

    static void respondToPolicyDecision(PolicyClientTest* test, WebKitPolicyDecision* decision)
    {
        switch (test->m_policyDecisionResponse) {
        case Use:
            webkit_policy_decision_use(decision);
            break;
        case UseWithPolicy:
            webkit_policy_decision_use_with_policies(decision, test->m_websitePolicies.get());
            break;
        case Ignore:
            webkit_policy_decision_ignore(decision);
            break;
        case Download:
            webkit_policy_decision_download(decision);
            break;
        case None:
            break;
        }

        if (test->m_haltMainLoopAfterMakingDecision)
            g_idle_add(reinterpret_cast<GSourceFunc>(quitMainLoopLater), test->m_mainLoop);
    }

    static gboolean respondToPolicyDecisionLater(PolicyClientTest* test)
    {
        respondToPolicyDecision(test, test->m_previousPolicyDecision.get());
        test->m_previousPolicyDecision = 0;
        return FALSE;
    }

    static gboolean decidePolicyCallback(WebKitWebView* webView, WebKitPolicyDecision* decision, WebKitPolicyDecisionType type, PolicyClientTest* test)
    {
        if (test->m_policyDecisionTypeFilter != type)
            return FALSE;

        test->m_previousPolicyDecision = decision;
        if (test->m_respondToPolicyDecisionAsynchronously) {
            g_idle_add(reinterpret_cast<GSourceFunc>(respondToPolicyDecisionLater), test);
            return TRUE;
        }

        respondToPolicyDecision(test, decision);

        // We return FALSE here to ensure that the default policy decision
        // handler doesn't override whatever we use here.
        return FALSE;
    }

    PolicyClientTest()
        : LoadTrackingTest()
    {
        g_signal_connect(m_webView, "decide-policy", G_CALLBACK(decidePolicyCallback), this);
        webkit_user_content_manager_register_script_message_handler(m_userContentManager.get(), "testHandler");
        g_signal_connect(m_userContentManager.get(), "script-message-received::testHandler", G_CALLBACK(testHandlerMessageReceivedCallback), this);
    }

    bool loadURIAndWaitForAutoPlayed(const char* uri, WebKitAutoplayPolicy policy)
    {
        m_autoplayed = WTF::nullopt;
        m_websitePolicies = adoptGRef(webkit_website_policies_new_with_policies("autoplay", policy, nullptr));
        m_policyDecisionResponse = PolicyClientTest::UseWithPolicy;

        loadURI(uri);
        // Run until the user content messages come back from the test HTML.
        g_main_loop_run(m_mainLoop);
        return m_autoplayed.valueOr(false);
    }

    bool runJSAndWaitForAutoPlayed(const char* script)
    {
        runJavaScriptWithoutForcedUserGesturesAndWaitUntilFinished(script, nullptr);

        // Spin until autoplay status is reported, the run JS API can
        // complete its main loop before the promises in autoplay-check fire.
        while (!m_autoplayed.hasValue())
            g_main_loop_run(m_mainLoop);

        return *m_autoplayed;
    }

    PolicyDecisionResponse m_policyDecisionResponse { None };
    int m_policyDecisionTypeFilter { 0 };
    bool m_respondToPolicyDecisionAsynchronously { false };
    bool m_haltMainLoopAfterMakingDecision { false };
    Optional<bool> m_autoplayed;
    GRefPtr<WebKitPolicyDecision> m_previousPolicyDecision;
    GRefPtr<WebKitWebsitePolicies> m_websitePolicies;
};

static void testNavigationPolicy(PolicyClientTest* test, gconstpointer)
{
    test->m_policyDecisionTypeFilter = WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION;

    test->m_policyDecisionResponse = PolicyClientTest::Use;
    test->loadHtml("<html/>", "http://webkitgtk.org/");
    test->waitUntilLoadFinished();
    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);

    // Ideally we'd like to have a more intensive test here, but it's still pretty tricky
    // to trigger different types of navigations with the GTK+ WebKit2 API.
    WebKitNavigationPolicyDecision* decision = WEBKIT_NAVIGATION_POLICY_DECISION(test->m_previousPolicyDecision.get());
    WebKitNavigationAction* navigationAction = webkit_navigation_policy_decision_get_navigation_action(decision);
    g_assert_cmpint(webkit_navigation_action_get_navigation_type(navigationAction), ==, WEBKIT_NAVIGATION_TYPE_OTHER);
    g_assert_cmpint(webkit_navigation_action_get_mouse_button(navigationAction), ==, 0);
    g_assert_cmpint(webkit_navigation_action_get_modifiers(navigationAction), ==, 0);
    g_assert_false(webkit_navigation_action_is_redirect(navigationAction));
    g_assert_null(webkit_navigation_policy_decision_get_frame_name(decision));
    WebKitURIRequest* request = webkit_navigation_action_get_request(navigationAction);
    g_assert_cmpstr(webkit_uri_request_get_uri(request), ==, "http://webkitgtk.org/");

    test->m_policyDecisionResponse = PolicyClientTest::Use;
    test->m_respondToPolicyDecisionAsynchronously = true;
    test->loadHtml("<html/>", "http://webkitgtk.org/");
    test->waitUntilLoadFinished();
    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);

    test->m_policyDecisionResponse = PolicyClientTest::Use;
    test->m_respondToPolicyDecisionAsynchronously = false;
    test->loadURI(kServer->getURIForPath("/redirect").data());
    test->waitUntilLoadFinished();
    g_assert_cmpint(test->m_loadEvents.size(), ==, 4);

    decision = WEBKIT_NAVIGATION_POLICY_DECISION(test->m_previousPolicyDecision.get());
    navigationAction = webkit_navigation_policy_decision_get_navigation_action(decision);
    g_assert_true(webkit_navigation_action_is_redirect(navigationAction));
    g_assert_null(webkit_navigation_policy_decision_get_frame_name(decision));
    request = webkit_navigation_action_get_request(navigationAction);
    g_assert_cmpstr(webkit_uri_request_get_uri(request), ==, kServer->getURIForPath("/").data());

    // If we are waiting until load completion, it will never complete if we ignore the
    // navigation. So we tell the main loop to quit sometime later.
    test->m_policyDecisionResponse = PolicyClientTest::Ignore;
    test->m_respondToPolicyDecisionAsynchronously = false;
    test->m_haltMainLoopAfterMakingDecision = true;
    test->loadHtml("<html/>", "http://webkitgtk.org/");
    test->waitUntilLoadFinished();
    g_assert_cmpint(test->m_loadEvents.size(), ==, 0);

    test->m_policyDecisionResponse = PolicyClientTest::Ignore;
    test->loadHtml("<html/>", "http://webkitgtk.org/");
    test->waitUntilLoadFinished();
    g_assert_cmpint(test->m_loadEvents.size(), ==, 0);
}

static void testResponsePolicy(PolicyClientTest* test, gconstpointer)
{
    test->m_policyDecisionTypeFilter = WEBKIT_POLICY_DECISION_TYPE_RESPONSE;

    test->m_policyDecisionResponse = PolicyClientTest::Use;
    test->loadURI(kServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);

    WebKitResponsePolicyDecision* decision = WEBKIT_RESPONSE_POLICY_DECISION(test->m_previousPolicyDecision.get());
    WebKitURIRequest* request = webkit_response_policy_decision_get_request(decision);
    g_assert_true(WEBKIT_IS_URI_REQUEST(request));
    ASSERT_CMP_CSTRING(webkit_uri_request_get_uri(request), ==, kServer->getURIForPath("/"));
    WebKitURIResponse* response = webkit_response_policy_decision_get_response(decision);
    g_assert_true(WEBKIT_IS_URI_RESPONSE(response));
    ASSERT_CMP_CSTRING(webkit_uri_response_get_uri(response), ==, kServer->getURIForPath("/"));
    g_assert_cmpint(webkit_web_view_can_show_mime_type(test->m_webView, webkit_uri_response_get_mime_type(response)), ==,
        webkit_response_policy_decision_is_mime_type_supported(decision));

    test->m_respondToPolicyDecisionAsynchronously = true;
    test->loadURI(kServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);

    test->m_respondToPolicyDecisionAsynchronously = false;
    test->m_policyDecisionResponse = PolicyClientTest::Ignore;
    test->loadURI(kServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();

    g_assert_cmpint(test->m_loadEvents.size(), ==, 3);
    g_assert_cmpint(test->m_loadEvents[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(test->m_loadEvents[1], ==, LoadTrackingTest::ProvisionalLoadFailed);
    g_assert_cmpint(test->m_loadEvents[2], ==, LoadTrackingTest::LoadFinished);
}

struct CreateCallbackData {
    bool triedToOpenWindow;
    GMainLoop* mainLoop;
};

static WebKitWebView* createCallback(WebKitWebView* webView, WebKitNavigationAction*, CreateCallbackData* data)
{
    data->triedToOpenWindow = true;
    g_main_loop_quit(data->mainLoop);
    return 0;
}

static void testNewWindowPolicy(PolicyClientTest* test, gconstpointer)
{
    static const char* windowOpeningHTML =
        "<html><body>"
        "    <a id=\"link\" href=\"http://www.google.com\" target=\"_blank\">Link</a>"
        "    <script>"
        "        var event = document.createEvent('MouseEvents');"
        "        event.initEvent('click', true, false);"
        "        document.getElementById('link').dispatchEvent(event);"
        "    </script>"
        "</body></html>";
    test->m_policyDecisionTypeFilter = WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION;
    webkit_settings_set_javascript_can_open_windows_automatically(webkit_web_view_get_settings(test->m_webView), TRUE);

    CreateCallbackData data;
    data.triedToOpenWindow = false;
    data.mainLoop = test->m_mainLoop;

    g_signal_connect(test->m_webView, "create", G_CALLBACK(createCallback), &data);
    test->m_policyDecisionResponse = PolicyClientTest::Use;
    test->loadHtml(windowOpeningHTML, "http://webkitgtk.org/");
    test->wait(1);
    g_assert_true(data.triedToOpenWindow);

    WebKitNavigationPolicyDecision* decision = WEBKIT_NAVIGATION_POLICY_DECISION(test->m_previousPolicyDecision.get());
    g_assert_cmpstr(webkit_navigation_policy_decision_get_frame_name(decision), ==, "_blank");

    // Using a short timeout is a bit ugly here, but it's hard to get around because if we block
    // the new window signal we cannot halt the main loop in the create callback. If we
    // halt the main loop in the policy decision, the create callback never executes.
    data.triedToOpenWindow = false;
    test->m_policyDecisionResponse = PolicyClientTest::Ignore;
    test->loadHtml(windowOpeningHTML, "http://webkitgtk.org/");
    test->wait(.2);
    g_assert_false(data.triedToOpenWindow);
}

static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    if (g_str_equal(path, "/")) {
        static const char* responseString = "<html><body>Testing!</body></html>";
        soup_message_set_status(message, SOUP_STATUS_OK);
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, responseString, strlen(responseString));
        soup_message_body_complete(message->response_body);
    } else if (g_str_equal(path, "/redirect")) {
        soup_message_set_status(message, SOUP_STATUS_MOVED_PERMANENTLY);
        soup_message_headers_append(message->response_headers, "Location", "/");
    } else
        soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
}

static void testAutoplayPolicy(PolicyClientTest* test, gconstpointer)
{
#if PLATFORM(GTK)
    // The web view must be realized for the video to start playback.
    test->showInWindowAndWaitUntilMapped(GTK_WINDOW_TOPLEVEL);
#endif

    test->m_policyDecisionTypeFilter = WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION;

    const char* resourceName = "autoplay-check.html";
    GUniquePtr<char> resourcePath(g_build_filename(Test::getResourcesDir(Test::WebKit2Resources).data(), resourceName, nullptr));
    GUniquePtr<char> resourceURL(g_filename_to_uri(resourcePath.get(), nullptr, nullptr));

    g_assert_false(test->loadURIAndWaitForAutoPlayed(resourceURL.get(), WEBKIT_AUTOPLAY_DENY));

    // Async test
    test->m_respondToPolicyDecisionAsynchronously = true;
    g_assert_false(test->loadURIAndWaitForAutoPlayed(resourceURL.get(), WEBKIT_AUTOPLAY_DENY));
    test->m_respondToPolicyDecisionAsynchronously = false;
    g_assert_true(test->loadURIAndWaitForAutoPlayed(resourceURL.get(), WEBKIT_AUTOPLAY_ALLOW));
    g_assert_false(test->loadURIAndWaitForAutoPlayed(resourceURL.get(), WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND));

    // Now check dynamically updating the loader policies
    g_assert_false(test->loadURIAndWaitForAutoPlayed(resourceURL.get(), WEBKIT_AUTOPLAY_DENY));
    // Check that JS can not side-step the active policy
    g_assert_false(test->runJSAndWaitForAutoPlayed("playVideo();"));

    // Silent audio track tests
    resourceName = "autoplay-no-audio-check.html";
    resourcePath.reset(g_build_filename(Test::getResourcesDir(Test::WebKit2Resources).data(), resourceName, nullptr));
    resourceURL.reset(g_filename_to_uri(resourcePath.get(), nullptr, nullptr));

    g_assert_false(test->loadURIAndWaitForAutoPlayed(resourceURL.get(), WEBKIT_AUTOPLAY_DENY));
    g_assert_true(test->loadURIAndWaitForAutoPlayed(resourceURL.get(), WEBKIT_AUTOPLAY_ALLOW));
    g_assert_true(test->loadURIAndWaitForAutoPlayed(resourceURL.get(), WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND));
}

void beforeAll()
{
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

    PolicyClientTest::add("WebKitPolicyClient", "navigation-policy", testNavigationPolicy);
    PolicyClientTest::add("WebKitPolicyClient", "response-policy", testResponsePolicy);
    PolicyClientTest::add("WebKitPolicyClient", "autoplay-policy", testAutoplayPolicy);
    // WARNING: This test must come last, it uses racey constructs that
    // interfere nondeterminisically with any test running after it.
    // https://bugs.webkit.org/show_bug.cgi?id=213190
    PolicyClientTest::add("WebKitPolicyClient", "new-window-policy", testNewWindowPolicy);
}

void afterAll()
{
    delete kServer;
}
