/*
 * Copyright (C) 2025 Igalia S.L.
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
#include <wtf/text/MakeString.h>

static WebKitTestServer* kHttpsServer = nullptr;

static const char indexHTML[] =
"<html><body>"
"<input id='enterXR' type=\"button\" value=\"click to enter experience\"/>"
"<script>"
"document.getElementById('enterXR').addEventListener('click', () => {"
"  navigator.xr.requestSession('immersive-vr').then(session => {"
"    console.log('XR session started');"
"    session.addEventListener('end', (event) => {"
"        console.log('XR session ended');"
"    });"
"  }).catch(err => console.error(`XR session failed to start: ${err}`));"
"});"
"</script></body></html>";

static WebKitFeature* findFeature(WebKitFeatureList *featureList, const char *identifier)
{
    for (gsize i = 0; i < webkit_feature_list_get_length(featureList); i++) {
        WebKitFeature* feature = webkit_feature_list_get(featureList, i);
        if (!g_ascii_strcasecmp(identifier, webkit_feature_get_identifier(feature)))
            return feature;
    }
    return nullptr;
}

static void relaxDMABufRequirement(WebKitSettings* settings)
{
    g_autoptr(WebKitFeatureList) featureList = webkit_settings_get_development_features();
    WebKitFeature* feature = findFeature(featureList, "OpenXRDMABufRelaxedForTesting");
    g_assert_nonnull(feature);
    webkit_settings_set_feature_enabled(settings, feature, true);
}

class WebXRTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(WebXRTest);
    WebXRTest();
    virtual ~WebXRTest() = default;

    static void isImmersiveModeEnabledChanged(GObject*, GParamSpec*, WebXRTest* test)
    {
        g_signal_handlers_disconnect_by_func(test->webView(), reinterpret_cast<void*>(isImmersiveModeEnabledChanged), test);
        g_main_loop_quit(test->m_mainLoop);
    }

    static gboolean permissionRequestCallback(WebKitWebView*, WebKitPermissionRequest *request, WebXRTest* test)
    {
        g_assert_true(WEBKIT_IS_XR_PERMISSION_REQUEST(request));
        g_assert_true(test->m_isExpectingPermissionRequest);

        webkit_permission_request_allow(request);

        g_signal_handlers_disconnect_by_func(test->webView(), reinterpret_cast<void*>(permissionRequestCallback), test);

        return TRUE;
    }

    void waitUntilIsImmersiveModeEnabledChanged()
    {
        g_signal_connect(m_webView.get(), "notify::is-immersive-mode-enabled", G_CALLBACK(isImmersiveModeEnabledChanged), this);
        g_main_loop_run(m_mainLoop);
    }

    void leaveImmersiveModeAndWaitUntilImmersiveModeChanged()
    {
        webkit_web_view_leave_immersive_mode(m_webView.get());

        if (webkit_web_view_is_immersive_mode_enabled(m_webView.get()))
            waitUntilIsImmersiveModeEnabledChanged();
    }

    void clickOnEnterXRButtonAndWaitUntilImmersiveModeChanged()
    {
        g_signal_connect(m_webView.get(), "permission-request", G_CALLBACK(permissionRequestCallback), this);

        m_isExpectingPermissionRequest = true;

        runJavaScriptAndWaitUntilFinished("document.getElementById('enterXR').focus()", nullptr);
        runJavaScriptAndWaitUntilFinished("document.getElementById('enterXR').click();", nullptr);

        if (!webkit_web_view_is_immersive_mode_enabled(m_webView.get()))
            waitUntilIsImmersiveModeEnabledChanged();
    }

    bool m_isExpectingPermissionRequest { false };
};

WebXRTest::WebXRTest()
{
    WebKitSettings* defaultSettings = webkit_web_view_get_settings(m_webView.get());
    relaxDMABufRequirement(defaultSettings);
}

static void serverCallback(SoupServer*, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
{
    g_assert(soup_server_message_get_method(message) == SOUP_METHOD_GET);

    if (g_str_equal(path, "/xr-session/")) {
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);

        auto* responseBody = soup_server_message_get_response_body(message);
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, indexHTML, strlen(indexHTML));
        soup_message_body_complete(responseBody);
    } else
        g_assert_not_reached();
}

static void testWebKitWebXRLeaveImmersiveModeAndWaitUntilImmersiveModeChanged(WebXRTest* test, gconstpointer)
{
    if (!g_getenv("WITH_OPENXR_RUNTIME")) {
        g_test_skip("Unable to run without an OpenXR runtime");
        return;
    }

    WebViewTest::NetworkPolicyGuard guard(test, WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    g_assert_false(webkit_web_view_is_immersive_mode_enabled(test->m_webView.get()));

    test->loadURI(kHttpsServer->getURIForPath("/xr-session/").data());
    test->waitUntilLoadFinished();
    test->showInWindow();

    test->clickOnEnterXRButtonAndWaitUntilImmersiveModeChanged();
    g_assert_true(webkit_web_view_is_immersive_mode_enabled(test->m_webView.get()));

    test->leaveImmersiveModeAndWaitUntilImmersiveModeChanged();
    g_assert_false(webkit_web_view_is_immersive_mode_enabled(test->m_webView.get()));
}

static void testWebKitXRPermissionRequest(WebXRTest* test, gconstpointer)
{
    if (!g_getenv("WITH_OPENXR_RUNTIME")) {
        g_test_skip("Unable to run without an OpenXR runtime");
        return;
    }

    enum class Answer {
        Deny,
        Allow,
    };
    constexpr auto noFeature = static_cast<WebKitXRSessionFeatures>(0);
    struct Result {
        bool didCallback { false };
        std::optional<WebKitXRSessionMode> mode;
        String origin;
        WebKitXRSessionFeatures grantedFeatures { noFeature };
        WebKitXRSessionFeatures consentRequiredFeatures { noFeature };
        WebKitXRSessionFeatures consentOptionalFeatures { noFeature };
        WebKitXRSessionFeatures requiredFeaturesRequested { noFeature };
        WebKitXRSessionFeatures optionalFeaturesRequested { noFeature };
        String title;
    };
    struct Data {
        WebViewTest* test { nullptr };
        Answer answer { Answer::Deny };
        Result result { };

        void resetResult()
        {
            result = Result();
        }
    } data { test };
    typedef gboolean (*PermissionRequestCallback)(WebKitWebView*, WebKitPermissionRequest*, Data*);
    PermissionRequestCallback permissionRequestCallback = [](WebKitWebView*, WebKitPermissionRequest* request, Data* data) -> gboolean {
        g_assert_true(WEBKIT_IS_XR_PERMISSION_REQUEST(request));
        WebKitXRPermissionRequest* xrRequest = WEBKIT_XR_PERMISSION_REQUEST(request);

        data->result.didCallback = true;
        data->result.mode = webkit_xr_permission_request_get_session_mode(xrRequest);
        g_autofree gchar* originStr = webkit_security_origin_to_string(webkit_xr_permission_request_get_security_origin(xrRequest));
        data->result.origin = String::fromUTF8(originStr);
        data->result.grantedFeatures = webkit_xr_permission_request_get_granted_features(xrRequest);
        data->result.consentRequiredFeatures = webkit_xr_permission_request_get_consent_required_features(xrRequest);
        data->result.consentOptionalFeatures = webkit_xr_permission_request_get_consent_optional_features(xrRequest);
        data->result.requiredFeaturesRequested = webkit_xr_permission_request_get_required_features_requested(xrRequest);
        data->result.optionalFeaturesRequested = webkit_xr_permission_request_get_optional_features_requested(xrRequest);

        if (data->answer == Answer::Deny)
            webkit_permission_request_deny(request);
        else
            webkit_permission_request_allow(request);
        return TRUE;
    };

    test->loadHtml("", "https://foo.com/bar");
    test->waitUntilLoadFinished();
    test->showInWindow();

    auto testPermissionRequest = [&](StringView mode, StringView options, Answer answer) {
        auto script = makeString(
            "async function start() {"
            "    try {"
            "        const session = await navigator.xr.requestSession('"_s, mode, "', {"_s, options, "});"
            "        session.end();"
            "        document.title = 'pass';"
            "    } catch (e) {"
            "        document.title = 'fail';"
            "    }"
            "}"
            "start()"_s);
        data.answer = answer;
        data.resetResult();
        test->runJavaScriptAndWaitUntilFinished(script.utf8().data(), nullptr);
        test->waitUntilTitleChanged();
        data.result.title = String::fromUTF8(webkit_web_view_get_title(test->webView()));
        test->runJavaScriptAndWaitUntilFinished("document.title = ''", nullptr);
    };

    // requestSession is rejected by default without a permission-request callback
    testPermissionRequest("immersive-vr"_s, ""_s, Answer::Allow);
    g_assert_false(data.result.didCallback);
    g_assert_cmpstr(data.result.title.utf8().data(), ==, "fail");

    // Register permission-request callback
    g_signal_connect(test->webView(), "permission-request", G_CALLBACK(permissionRequestCallback), &data);

    // WebKit grants an inline session without a permission request.
    testPermissionRequest("inline"_s, ""_s, Answer::Deny);
    g_assert_false(data.result.didCallback);
    g_assert_cmpstr(data.result.title.utf8().data(), ==, "pass");

    testPermissionRequest("immersive-vr"_s, ""_s, Answer::Deny);
    g_assert_true(data.result.didCallback);
    g_assert_cmpint(data.result.mode.value(), ==, WEBKIT_XR_SESSION_MODE_IMMERSIVE_VR);
    g_assert_cmpstr(data.result.origin.utf8().data(), ==, "https://foo.com");
    g_assert_cmpint(data.result.grantedFeatures, ==, WEBKIT_XR_SESSION_FEATURES_VIEWER | WEBKIT_XR_SESSION_FEATURES_LOCAL);
    g_assert_cmpint(data.result.consentRequiredFeatures, ==, noFeature);
    g_assert_cmpint(data.result.consentOptionalFeatures, ==, noFeature);
    g_assert_cmpint(data.result.requiredFeaturesRequested, ==, WEBKIT_XR_SESSION_FEATURES_VIEWER | WEBKIT_XR_SESSION_FEATURES_LOCAL);
    g_assert_cmpint(data.result.optionalFeaturesRequested, ==, noFeature);
    g_assert_cmpstr(data.result.title.utf8().data(), ==, "fail");

    // Monado doesn't support hand-tracking
    testPermissionRequest("immersive-ar"_s, "requiredFeatures: ['local', 'unbounded'], optionalFeatures: ['hand-tracking']"_s, Answer::Allow);
    g_assert_true(data.result.didCallback);
    g_assert_cmpint(data.result.mode.value(), ==, WEBKIT_XR_SESSION_MODE_IMMERSIVE_AR);
    g_assert_cmpstr(data.result.origin.utf8().data(), ==, "https://foo.com");
    g_assert_cmpint(data.result.grantedFeatures, ==, WEBKIT_XR_SESSION_FEATURES_VIEWER | WEBKIT_XR_SESSION_FEATURES_LOCAL | WEBKIT_XR_SESSION_FEATURES_UNBOUNDED);
    g_assert_cmpint(data.result.consentRequiredFeatures, ==, noFeature);
    g_assert_cmpint(data.result.consentOptionalFeatures, ==, noFeature);
    g_assert_cmpint(data.result.requiredFeaturesRequested, ==, WEBKIT_XR_SESSION_FEATURES_VIEWER | WEBKIT_XR_SESSION_FEATURES_LOCAL | WEBKIT_XR_SESSION_FEATURES_UNBOUNDED);
    g_assert_cmpint(data.result.optionalFeaturesRequested, ==, noFeature);
    g_assert_cmpstr(data.result.title.utf8().data(), ==, "pass");
}

void beforeAll()
{
    kHttpsServer = new WebKitTestServer(WebKitTestServer::ServerHTTPS);
    kHttpsServer->run(serverCallback);

    WebXRTest::add("WebKitWebXR", "leave-immersive-mode", testWebKitWebXRLeaveImmersiveModeAndWaitUntilImmersiveModeChanged);
    WebXRTest::add("WebKitWebXR", "permission-request", testWebKitXRPermissionRequest);
}

void afterAll()
{
    delete kHttpsServer;
}
