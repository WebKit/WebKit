/*
 * Copyright (C) 2017 Igalia S.L.
 * Copyright (C) 2012 Samsung Electronics Ltd. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "WebViewTest.h"
#include <signal.h>
#include <sys/eventfd.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/WTFString.h>

static GPid gActiveServerPid;
static const unsigned gMaxConnectionTries = 10;

class InspectorServerTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(InspectorServerTest);

    InspectorServerTest()
    {
        startTestServer();
        g_assert_cmpuint(m_connectionTryCount, <, gMaxConnectionTries);
    }

    ~InspectorServerTest()
    {
        stopTestServer();
    }

    void connectToInspectorServer()
    {
        if (++m_connectionTryCount == gMaxConnectionTries) {
            g_main_loop_quit(m_mainLoop);
            return;
        }

        GRefPtr<GSocketClient> socketClient = adoptGRef(g_socket_client_new());
        g_socket_client_connect_to_host_async(socketClient.get(), "127.0.0.1:2999", 0, nullptr, [](GObject* client, GAsyncResult* result, gpointer userData) {
            auto* test = static_cast<InspectorServerTest*>(userData);
            GUniqueOutPtr<GError> error;
            GRefPtr<GSocketConnection> connection = adoptGRef(g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(client), result, &error.outPtr()));
            if (!connection) {
                if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED)) {
                    g_timeout_add(100, [](gpointer userData) {
                        static_cast<InspectorServerTest*>(userData)->connectToInspectorServer();
                        return G_SOURCE_REMOVE;
                    }, userData);
                    return;
                }

                g_printerr("Failed to connect to inspector server\n");
                g_assert_not_reached();
            }
            g_main_loop_quit(test->m_mainLoop);
        }, this);
    }

    void startTestServer()
    {
        // Prepare argv[] for spawning the server process.
        GUniquePtr<char> testServerPath(g_build_filename(WEBKIT_EXEC_PATH, "TestWebKitAPI", "WebKitGTK", "InspectorTestServer", nullptr));

        int eventFD = eventfd(0, 0);
        g_assert_cmpint(eventFD, !=, -1);
        GUniquePtr<gchar> eventFDString(g_strdup_printf("%d", eventFD));

        char* testServerArgv[3];
        testServerArgv[0] = testServerPath.get();
        testServerArgv[1] = eventFDString.get();
        testServerArgv[2] = nullptr;

        GUniquePtr<char*> testServerEnvp(g_environ_setenv(g_get_environ(), shouldStartHTTPServer ? "WEBKIT_INSPECTOR_HTTP_SERVER" : "WEBKIT_INSPECTOR_SERVER", "127.0.0.1:2999", TRUE));

        g_assert_true(g_spawn_async(nullptr, testServerArgv, testServerEnvp.get(), G_SPAWN_LEAVE_DESCRIPTORS_OPEN, nullptr, nullptr, &m_serverPid, nullptr));

        gActiveServerPid = m_serverPid;

        // Wait for the page to load.
        uint64_t event;
        auto result = read(eventFD, &event, sizeof(uint64_t));
        g_assert_cmpint(result, ==, sizeof(uint64_t));
        close(eventFD);

        // Wait until server is ready.
        connectToInspectorServer();
        g_main_loop_run(m_mainLoop);
        if (m_connectionTryCount == gMaxConnectionTries)
            stopTestServer();
    }

    void stopTestServer()
    {
        // Do nothing if there's no server running.
        if (!m_serverPid)
            return;

        g_spawn_close_pid(m_serverPid);
        kill(m_serverPid, SIGTERM);
        m_serverPid = 0;
        gActiveServerPid = 0;
    }

    bool loadTargetListPageAndWaitUntilFinished()
    {
        loadURI(shouldStartHTTPServer ? "http://127.0.0.1:2999/" : "inspector://127.0.0.1:2999");
        waitUntilLoadFinished();

        for (unsigned i = 0; i < 5; ++i) {
            size_t mainResourceDataSize = 0;
            const char* mainResourceData = this->mainResourceData(mainResourceDataSize);
            g_assert_nonnull(mainResourceData);
            if (g_strrstr_len(mainResourceData, mainResourceDataSize, "No targets found")) {
                webkit_web_view_reload(m_webView);
                waitUntilLoadFinished();
                continue;
            }

            if (g_strrstr_len(mainResourceData, mainResourceDataSize, "Inspectable targets"))
                return true;
        }

        return false;
    }

    GPid m_serverPid { 0 };
    unsigned m_connectionTryCount { 0 };
    static bool shouldStartHTTPServer;
};

bool InspectorServerTest::shouldStartHTTPServer = false;

class InspectorHTTPServerTest : public InspectorServerTest {
public:
    MAKE_GLIB_TEST_FIXTURE_WITH_SETUP_TEARDOWN(InspectorHTTPServerTest, setup, teardown);

    static void setup()
    {
        InspectorServerTest::shouldStartHTTPServer = true;
    }

    static void teardown()
    {
        InspectorServerTest::shouldStartHTTPServer = false;
    }
};

// Test to get inspector server page list from the test server.
// Should contain only one entry pointing to http://127.0.0.1:2999.
static void testInspectorServerPageList(InspectorServerTest* test, gconstpointer)
{
    test->showInWindow();
    g_assert_true(test->loadTargetListPageAndWaitUntilFinished());

    GUniqueOutPtr<GError> error;
    WebKitJavascriptResult* javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementsByClassName('targetname').length;", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    g_assert_cmpint(WebViewTest::javascriptResultToNumber(javascriptResult), ==, 1);

    GUniquePtr<char> valueString;
    javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementsByClassName('targeturl')[0].innerText;", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(valueString.get(), ==, "http://127.0.0.1:2999/");

    javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementsByTagName('input')[0].onclick.toString();", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_nonnull(g_strrstr(valueString.get(), "window.webkit.messageHandlers.inspector.postMessage('1:1:WebPage');"));
}

// Test to get inspector server page list from the HTTP test server.
// Should contain only one entry pointing to http://127.0.0.1:2999.
static void testInspectorHTTPServerPageList(InspectorHTTPServerTest* test, gconstpointer)
{
    test->showInWindow();
    g_assert_true(test->loadTargetListPageAndWaitUntilFinished());

    GUniqueOutPtr<GError> error;
    WebKitJavascriptResult* javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementsByClassName('targetname').length;", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    g_assert_cmpint(WebViewTest::javascriptResultToNumber(javascriptResult), ==, 1);

    GUniquePtr<char> valueString;
    javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementsByClassName('targeturl')[0].innerText;", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(valueString.get(), ==, "http://127.0.0.1:2999/");

    javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementsByTagName('input')[0].onclick.toString();", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_nonnull(g_strrstr(valueString.get(), "window.open('Main.html?ws=' + window.location.host + '/socket/1/1/WebPage', '_blank', 'location=no,menubar=no,status=no,toolbar=no');"));
}

void beforeAll()
{
    // Terminate the server process if any test fails.
    signal(SIGABRT, +[](int) {
        kill(gActiveServerPid, SIGTERM);
    });

    InspectorServerTest::add("WebKitWebInspectorServer", "test-page-list", testInspectorServerPageList);
    InspectorHTTPServerTest::add("WebKitWebInspectorServer", "http-test-page-list", testInspectorHTTPServerPageList);
}

void afterAll()
{
}
