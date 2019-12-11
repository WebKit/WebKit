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
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/WTFString.h>

static GPid gChildProcessPid;
static bool gServerIsReady;

static void stopTestServer()
{
    // Do nothing if there's no server running.
    if (!gChildProcessPid)
        return;

    g_spawn_close_pid(gChildProcessPid);
    kill(gChildProcessPid, SIGTERM);
    gChildProcessPid = 0;
}

static void sigAbortHandler(int sigNum)
{
    // Just stop the test server if SIGABRT was received.
    stopTestServer();
}

static void connectToInspectorServer(GMainLoop* mainLoop)
{
    g_dbus_connection_new_for_address("tcp:host=127.0.0.1,port=2999", G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, nullptr, nullptr,
        [](GObject* , GAsyncResult* result, gpointer userData) {
            auto* mainLoop = static_cast<GMainLoop*>(userData);
            GUniqueOutPtr<GError> error;
            GRefPtr<GDBusConnection> connection = adoptGRef(g_dbus_connection_new_for_address_finish(result, &error.outPtr()));
            if (!connection) {
                if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED)) {
                    g_timeout_add(100, [](gpointer userData) {
                        connectToInspectorServer(static_cast<GMainLoop*>(userData));
                        return G_SOURCE_REMOVE;
                    }, userData);
                    return;
                }

                g_printerr("Failed to connect to inspector server");
                g_assert_not_reached();
            }
            gServerIsReady = true;
            g_main_loop_quit(mainLoop);
    }, mainLoop);
}

static void waitUntilInspectorServerIsReady()
{
    GRefPtr<GMainLoop> mainLoop = adoptGRef(g_main_loop_new(nullptr, FALSE));
    unsigned timeoutID = g_timeout_add(25000, [](gpointer userData) {
        g_main_loop_quit(static_cast<GMainLoop*>(userData));
        return G_SOURCE_REMOVE;
    }, mainLoop.get());
    connectToInspectorServer(mainLoop.get());
    g_main_loop_run(mainLoop.get());
    if (gServerIsReady)
        g_source_remove(timeoutID);
}

static void startTestServer()
{
    // Prepare argv[] for spawning the server process.
    GUniquePtr<char> testServerPath(g_build_filename(WEBKIT_EXEC_PATH, "TestWebKitAPI", "WebKit2Gtk", "InspectorTestServer", nullptr));

    // We install a handler to ensure that we kill the child process
    // if the parent dies because of whatever the reason is.
    signal(SIGABRT, sigAbortHandler);

    char* testServerArgv[2];
    testServerArgv[0] = testServerPath.get();
    testServerArgv[1] = nullptr;

    g_assert_true(g_spawn_async(nullptr, testServerArgv, nullptr, G_SPAWN_DEFAULT, nullptr, nullptr, &gChildProcessPid, nullptr));

    waitUntilInspectorServerIsReady();
    if (!gServerIsReady)
        stopTestServer();
}

class InspectorServerTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(InspectorServerTest);

    bool getPageList()
    {
        loadURI("inspector://127.0.0.1:2999");
        waitUntilLoadFinished();
        for (unsigned i = 0; i < 5; ++i) {
            size_t mainResourceDataSize = 0;
            const char* mainResourceData = this->mainResourceData(mainResourceDataSize);
            g_assert_nonnull(mainResourceData);
            if (g_strrstr_len(mainResourceData, mainResourceDataSize, "No targets found"))
                wait(0.1);
            else if (g_strrstr_len(mainResourceData, mainResourceDataSize, "Inspectable targets"))
                return true;
        }

        return false;
    }
};

// Test to get inspector server page list from the test server.
// Should contain only one entry pointing to http://127.0.0.1:2999.
static void testInspectorServerPageList(InspectorServerTest* test, gconstpointer)
{
    GUniqueOutPtr<GError> error;

    test->showInWindowAndWaitUntilMapped(GTK_WINDOW_TOPLEVEL);
    g_assert_true(test->getPageList());

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
}

void beforeAll()
{
    startTestServer();
    InspectorServerTest::add("WebKitWebInspectorServer", "test-page-list", testInspectorServerPageList);
}

void afterAll()
{
    stopTestServer();
}
