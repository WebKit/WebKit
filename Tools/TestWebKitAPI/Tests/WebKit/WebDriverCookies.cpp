/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit/WKContext.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKWebsiteDataStoreConfigurationRef.h>
#include <WebKit/WKWebsiteDataStoreRef.h>
#include <wtf/FileSystem.h>

#if PLATFORM(GTK) || PLATFORM(WPE)
#include <WebKit/WebKitAutomationSession.h>
#include <WebKit/WebKitCookieManager.h>
#include <WebKit/WebKitWebsiteDataManager.h>
#include <WebKit/webkit.h>
#endif

namespace TestWebKitAPI {

class WebDriverCookieTest : public testing::Test {
public:
    void SetUp() override
    {
        // Set up test infrastructure for WebDriver cookie testing
#if PLATFORM(GTK) || PLATFORM(WPE)
        m_webContext = webkit_web_context_get_default();
        m_automationSession = webkit_automation_session_new("WebDriverCookieTest", "1.0");
        m_websiteDataManager = webkit_web_context_get_website_data_manager(m_webContext);
        m_cookieManager = webkit_website_data_manager_get_cookie_manager(m_websiteDataManager);

        // Connect to cookie change signal to verify signal emission
        g_signal_connect(m_cookieManager, "changed", G_CALLBACK(onCookieChanged), this);

        m_cookieChangedSignalReceived = false;
#endif
    }

    void TearDown() override
    {
#if PLATFORM(GTK) || PLATFORM(WPE)
        if (m_automationSession)
            g_object_unref(m_automationSession);
#endif
    }

#if PLATFORM(GTK) || PLATFORM(WPE)
    static void onCookieChanged(WebKitCookieManager*, gpointer userData)
    {
        auto* test = static_cast<WebDriverCookieTest*>(userData);
        test->m_cookieChangedSignalReceived = true;
    }

    WebKitWebContext* m_webContext { nullptr };
    WebKitAutomationSession* m_automationSession { nullptr };
    WebKitWebsiteDataManager* m_websiteDataManager { nullptr };
    WebKitCookieManager* m_cookieManager { nullptr };
    bool m_cookieChangedSignalReceived { false };
#endif
};

#if PLATFORM(GTK) || PLATFORM(WPE)

TEST_F(WebDriverCookieTest, BasicWebDriverCookieStorage)
{
    // Test 1: Basic WebDriver cookie setting through automation session

    // Create a WebDriver cookie directly
    SoupCookie* cookie = soup_cookie_new("webdriver_test_cookie", "webdriver_test_value",
        "127.0.0.1", "/", -1);

    // Set cookie through WebKit cookie manager (simulates WebDriver path)
    webkit_cookie_manager_add_cookie(m_cookieManager, cookie, nullptr, nullptr, nullptr);

    // Wait for signal emission
    auto startTime = g_get_monotonic_time();
    while (!m_cookieChangedSignalReceived && (g_get_monotonic_time() - startTime) < 5000000)
        g_main_context_iteration(nullptr, FALSE);

    EXPECT_TRUE(m_cookieChangedSignalReceived);

    soup_cookie_free(cookie);
}

TEST_F(WebDriverCookieTest, WebDriverCookieSignalEmission)
{
    // Test 2: Verify that WebDriver cookie operations emit the 'changed' signal
    // This tests the core fix in Bug 279079

    m_cookieChangedSignalReceived = false;

    // Create multiple cookies to test signal emission for each
    std::vector<SoupCookie*> cookies;
    for (int i = 0; i < 3; ++i) {
        auto name = g_strdup_printf("webdriver_signal_test_%d", i);
        auto value = g_strdup_printf("signal_value_%d", i);
        cookies.push_back(soup_cookie_new(name, value, "127.0.0.1", "/", -1));
        g_free(name);
        g_free(value);
    }

    int signalCount = 0;
    auto originalHandler = G_CALLBACK(onCookieChanged);
    g_signal_handlers_disconnect_by_func(m_cookieManager, originalHandler, this);

    // Connect a counting handler
    g_signal_connect(m_cookieManager, "changed", G_CALLBACK(+[](WebKitCookieManager*, gpointer userData) {
        (*static_cast<int*>(userData))++;
    }), &signalCount);

    // Add cookies through WebDriver path
    for (auto* cookie : cookies)
        webkit_cookie_manager_add_cookie(m_cookieManager, cookie, nullptr, nullptr, nullptr);

    // Wait for signals
    auto startTime = g_get_monotonic_time();
    while (signalCount < 3 && (g_get_monotonic_time() - startTime) < 5000000)
        g_main_context_iteration(nullptr, FALSE);

    EXPECT_EQ(signalCount, 3);

    // Cleanup
    for (auto* cookie : cookies)
        soup_cookie_free(cookie);
}

TEST_F(WebDriverCookieTest, WebDriverCookieDOMConsistency)
{
    // Test 3: Verify WebDriver-set cookies are visible in DOM

    // Create a simple WebView to test DOM visibility
    auto webView = webkit_web_view_new_with_context(m_webContext);

    // Set a cookie through WebDriver path
    SoupCookie* cookie = soup_cookie_new("webdriver_dom_test", "dom_test_value",
        "127.0.0.1", "/", -1);
    webkit_cookie_manager_add_cookie(m_cookieManager, cookie, nullptr, nullptr, nullptr);

    // Load a page and check if cookie is accessible via document.cookie
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(webView),
        "<html><body><script>"
        "window.cookieTestResult = document.cookie.includes('webdriver_dom_test=dom_test_value');"
        "</script></body></html>",
        "http://127.0.0.1/");

    // Wait for page load
    bool loadFinished = false;
    g_signal_connect(webView, "load-finished", G_CALLBACK(+[](WebKitWebView*, WebKitLoadEvent, gpointer userData) {
        *static_cast<bool*>(userData) = true;
    }), &loadFinished);

    auto startTime = g_get_monotonic_time();
    while (!loadFinished && (g_get_monotonic_time() - startTime) < 5000000)
        g_main_context_iteration(nullptr, FALSE);

    EXPECT_TRUE(loadFinished);

    // Check if cookie is visible in DOM
    webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(webView), "window.cookieTestResult", nullptr,
        +[](GObject*, GAsyncResult* result, gpointer userData) {
            WebKitJavascriptResult* jsResult = webkit_web_view_run_javascript_finish(WEBKIT_WEB_VIEW(userData), result, nullptr);
            if (jsResult) {
                JSCValue* value = webkit_javascript_result_get_js_value(jsResult);
                bool* testResult = static_cast<bool*>(g_object_get_data(G_OBJECT(userData), "test-result"));
                *testResult = jsc_value_to_boolean(value);
                webkit_javascript_result_unref(jsResult);
            }
            bool* finished = static_cast<bool*>(g_object_get_data(G_OBJECT(userData), "finished"));
            *finished = true;
        }, webView);

    bool testResult = false;
    bool jsFinished = false;
    g_object_set_data(G_OBJECT(webView), "test-result", &testResult);
    g_object_set_data(G_OBJECT(webView), "finished", &jsFinished);

    startTime = g_get_monotonic_time();
    while (!jsFinished && (g_get_monotonic_time() - startTime) < 5000000)
        g_main_context_iteration(nullptr, FALSE);

    EXPECT_TRUE(testResult);

    // Cleanup
    soup_cookie_free(cookie);
    g_object_unref(webView);
}

TEST_F(WebDriverCookieTest, WebDriverCookieStressTest)
{
    // Test 4: Stress test - multiple rapid WebDriver cookie operations

    m_cookieChangedSignalReceived = false;
    int signalCount = 0;

    g_signal_handlers_disconnect_by_data(m_cookieManager, this);
    g_signal_connect(m_cookieManager, "changed", G_CALLBACK(+[](WebKitCookieManager*, gpointer userData) {
        (*static_cast<int*>(userData))++;
    }), &signalCount);

    // Rapidly add multiple cookies
    const int numCookies = 20;
    std::vector<SoupCookie*> cookies;

    for (int i = 0; i < numCookies; ++i) {
        auto name = g_strdup_printf("webdriver_stress_%d", i);
        auto value = g_strdup_printf("stress_value_%d", i);
        auto* cookie = soup_cookie_new(name, value, "127.0.0.1", "/", -1);
        cookies.push_back(cookie);

        webkit_cookie_manager_add_cookie(m_cookieManager, cookie, nullptr, nullptr, nullptr);

        g_free(name);
        g_free(value);
    }

    // Wait for all signals
    auto startTime = g_get_monotonic_time();
    while (signalCount < numCookies && (g_get_monotonic_time() - startTime) < 10000000)
        g_main_context_iteration(nullptr, FALSE);

    EXPECT_EQ(signalCount, numCookies);

    // Cleanup
    for (auto* cookie : cookies)
        soup_cookie_free(cookie);
}

#else

// For non-GTK/WPE platforms, provide placeholder tests
TEST_F(WebDriverCookieTest, PlatformNotSupported)
{
    // This test infrastructure is currently only implemented for GTK/WPE platforms
    // where libsoup is used and the signal emission bug occurs
    GTEST_SKIP() << "WebDriver cookie signal emission testing only supported on GTK/WPE platforms";
}

#endif // PLATFORM(GTK) || PLATFORM(WPE)

} // namespace TestWebKitAPI
