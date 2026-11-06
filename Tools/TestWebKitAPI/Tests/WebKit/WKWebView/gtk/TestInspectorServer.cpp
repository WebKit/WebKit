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
#include <sys/wait.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/SocketConnection.h>
#include <wtf/text/WTFString.h>

#if OS(LINUX)
#include <sys/prctl.h>
#endif

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
        if (m_clientConnection)
            m_clientConnection->close();
        if (m_observerConnection)
            m_observerConnection->close();
        stopTestServer();
    }

    void connectToInspectorServer()
    {
        if (++m_connectionTryCount == gMaxConnectionTries) {
            g_main_loop_quit(m_mainLoop);
            return;
        }

        GRefPtr<GSocketClient> socketClient = adoptGRef(g_socket_client_new());
        g_socket_client_connect_to_host_async(socketClient.get(), m_serverAddress.get(), 0, nullptr, [](GObject* client, GAsyncResult* result, gpointer userData) {
            auto* test = static_cast<InspectorServerTest*>(userData);
            GUniqueOutPtr<GError> error;
            GRefPtr<GSocketConnection> connection = adoptGRef(g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(client), result, &error.outPtr()));
            if (!connection) {
                if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED)) {
                    g_timeout_add(100, [](gpointer userData) -> gboolean {
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

        m_serverAddress.reset(g_strdup_printf("127.0.0.1:%u", availablePort()));
        GUniquePtr<char*> testServerEnvp(g_environ_setenv(g_get_environ(), shouldStartHTTPServer ? "WEBKIT_INSPECTOR_HTTP_SERVER" : "WEBKIT_INSPECTOR_SERVER", m_serverAddress.get(), TRUE));

        auto childSetup = [](gpointer) {
#if OS(LINUX)
            prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
        };
        g_assert_true(g_spawn_async(nullptr, testServerArgv, testServerEnvp.get(), static_cast<GSpawnFlags>(G_SPAWN_LEAVE_DESCRIPTORS_OPEN | G_SPAWN_DO_NOT_REAP_CHILD), childSetup, nullptr, &m_serverPid, nullptr));

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

        kill(m_serverPid, SIGTERM);
        waitpid(m_serverPid, nullptr, 0);
        g_spawn_close_pid(m_serverPid);
        m_serverPid = 0;
        gActiveServerPid = 0;
    }

    bool loadTargetListPageAndWaitUntilFinished()
    {
        GUniquePtr<char> uri(g_strdup_printf(shouldStartHTTPServer ? "http://%s/" : "inspector://%s", m_serverAddress.get()));
        loadURI(uri.get());
        waitUntilLoadFinished();

        for (unsigned i = 0; i < 5; ++i) {
            size_t mainResourceDataSize = 0;
            const char* mainResourceData = this->mainResourceData(mainResourceDataSize);
            g_assert_nonnull(mainResourceData);
            if (g_strrstr_len(mainResourceData, mainResourceDataSize, "No targets found")) {
                webkit_web_view_reload(m_webView.get());
                waitUntilLoadFinished();
                continue;
            }

            if (g_strrstr_len(mainResourceData, mainResourceDataSize, "Inspectable targets"))
                return true;
        }

        return false;
    }

    static unsigned availablePort()
    {
        GRefPtr<GSocket> socket = adoptGRef(g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, nullptr));
        GRefPtr<GInetAddress> loopback = adoptGRef(g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4));
        GRefPtr<GSocketAddress> address = adoptGRef(g_inet_socket_address_new(loopback.get(), 0));
        g_assert_true(g_socket_bind(socket.get(), address.get(), FALSE, nullptr));
        GRefPtr<GSocketAddress> boundAddress = adoptGRef(g_socket_get_local_address(socket.get(), nullptr));
        return g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(boundAddress.get()));
    }

    Ref<SocketConnection> connectToServer(const SocketConnection::MessageHandlers& messageHandlers)
    {
        GRefPtr<GSocketClient> socketClient = adoptGRef(g_socket_client_new());
        GUniqueOutPtr<GError> error;
        GRefPtr<GSocketConnection> connection = adoptGRef(g_socket_client_connect_to_host(socketClient.get(), m_serverAddress.get(), 0, nullptr, &error.outPtr()));
        g_assert_no_error(error.get());
        return SocketConnection::create(WTF::move(connection), messageHandlers, this);
    }

    void connectAsInspectorClient()
    {
        m_clientConnection = connectToServer(s_clientMessageHandlers);
    }

    void setupInspectorClientAndWaitForTarget()
    {
        m_clientConnection->sendMessage("SetupInspectorClient"_s, g_variant_new("(@ay)", g_variant_new_bytestring("")));
        while (!m_targetID)
            g_main_loop_run(m_mainLoop);
    }

    void waitForFrontendMessage(const char* substring)
    {
        m_expectedFrontendMessage = substring;
        g_main_loop_run(m_mainLoop);
        m_expectedFrontendMessage = nullptr;
    }

    void waitForTargetList(uint64_t targetID)
    {
        m_expectedTargetID = targetID;
        m_expectedTargetConnectionID = 0;
        while (!m_expectedTargetConnectionID)
            g_main_loop_run(m_mainLoop);
        m_expectedTargetID = 0;
    }

    void connectObserverAndWaitForAutomationSession()
    {
        m_observerConnection = connectToServer(s_observerMessageHandlers);
        m_observerConnection->sendMessage("StartAutomationSession"_s, g_variant_new("(s@a{sv})", "observer", g_variant_new_array(G_VARIANT_TYPE("{sv}"), nullptr, 0)));
        while (!m_automationSessionStarted)
            g_main_loop_run(m_mainLoop);
    }

    void waitForConnectionClosed(uint64_t connectionID)
    {
        m_expectedClosedConnectionID = connectionID;
        while (m_expectedClosedConnectionID)
            g_main_loop_run(m_mainLoop);
    }

    static const SocketConnection::MessageHandlers s_clientMessageHandlers;
    static const SocketConnection::MessageHandlers s_observerMessageHandlers;
    static const SocketConnection::MessageHandlers s_secondClientMessageHandlers;
    RefPtr<SocketConnection> m_observerConnection;
    bool m_automationSessionStarted { false };
    uint64_t m_expectedClosedConnectionID { 0 };
    RefPtr<SocketConnection> m_clientConnection;
    uint64_t m_connectionID { 0 };
    uint64_t m_targetID { 0 };
    uint64_t m_otherConnectionID { 0 };
    bool m_secondClientWasAccepted { false };
    const char* m_expectedFrontendMessage { nullptr };
    const char* m_unexpectedFrontendMessage { nullptr };
    bool m_receivedUnexpectedFrontendMessage { false };
    uint64_t m_observedTargetListConnectionID { 0 };
    uint64_t m_expectedTargetID { 0 };
    uint64_t m_expectedTargetConnectionID { 0 };

    GPid m_serverPid { 0 };
    GUniquePtr<char> m_serverAddress;
    unsigned m_connectionTryCount { 0 };
    static bool shouldStartHTTPServer;
};

bool InspectorServerTest::shouldStartHTTPServer = false;

const SocketConnection::MessageHandlers InspectorServerTest::s_clientMessageHandlers = {
    { "DidClose"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { { },
        [](SocketConnection&, GVariant*, gpointer) {
            g_printerr("Inspector server closed the connection\n");
            g_assert_not_reached();
        } }
    },
    { "SetTargetList"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { "(ta(tsssb))"_s,
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto* test = static_cast<InspectorServerTest*>(userData);
            guint64 connectionID;
            GRefPtr<GVariant> targetList;
            g_variant_get(parameters, "(t@a(tsssb))", &connectionID, &targetList.outPtr());
            if (test->m_expectedTargetID) {
                for (gsize i = 0; i < g_variant_n_children(targetList.get()); ++i) {
                    guint64 targetID;
                    g_variant_get_child(targetList.get(), i, "(tsssb)", &targetID, nullptr, nullptr, nullptr, nullptr);
                    if (targetID == test->m_expectedTargetID) {
                        test->m_expectedTargetConnectionID = connectionID;
                        g_main_loop_quit(test->m_mainLoop);
                    }
                }
                return;
            }
            if (test->m_targetID && connectionID != test->m_connectionID) {
                if (!test->m_otherConnectionID) {
                    test->m_otherConnectionID = connectionID;
                    g_main_loop_quit(test->m_mainLoop);
                }
                return;
            }
            if (test->m_targetID || !g_variant_n_children(targetList.get()))
                return;
            g_variant_get_child(targetList.get(), 0, "(tsssb)", &test->m_targetID, nullptr, nullptr, nullptr, nullptr);
            test->m_connectionID = connectionID;
            g_main_loop_quit(test->m_mainLoop);
        } }
    },
    { "SendMessageToFrontend"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { "(tts)"_s,
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto* test = static_cast<InspectorServerTest*>(userData);
            const char* message;
            g_variant_get(parameters, "(tt&s)", nullptr, nullptr, &message);
            if (test->m_unexpectedFrontendMessage && g_strstr_len(message, -1, test->m_unexpectedFrontendMessage))
                test->m_receivedUnexpectedFrontendMessage = true;
            if (test->m_expectedFrontendMessage && g_strstr_len(message, -1, test->m_expectedFrontendMessage))
                g_main_loop_quit(test->m_mainLoop);
        } }
    }
};

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
    JSCValue* value = test->runJavaScriptAndWaitUntilFinished("document.getElementsByClassName('targetname').length;", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    g_assert_cmpint(WebViewTest::javascriptResultToNumber(value), ==, 1);

    GUniquePtr<char> valueString;
    value = test->runJavaScriptAndWaitUntilFinished("document.getElementsByClassName('targeturl')[0].innerText;", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(value));
    g_assert_cmpstr(valueString.get(), ==, "http://127.0.0.1:2999/");

    value = test->runJavaScriptAndWaitUntilFinished("document.getElementsByTagName('input')[0].onclick.toString();", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(value));
    g_assert_nonnull(g_strrstr(valueString.get(), "window.webkit.messageHandlers.inspector.postMessage('1:1:WebPage')"));
}

// Test to get inspector server page list from the HTTP test server.
// Should contain only one entry pointing to http://127.0.0.1:2999.
static void testInspectorHTTPServerPageList(InspectorHTTPServerTest* test, gconstpointer)
{
    test->showInWindow();
    g_assert_true(test->loadTargetListPageAndWaitUntilFinished());

    GUniqueOutPtr<GError> error;
    JSCValue* value = test->runJavaScriptAndWaitUntilFinished("document.getElementsByClassName('targetname').length;", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    g_assert_cmpint(WebViewTest::javascriptResultToNumber(value), ==, 1);

    GUniquePtr<char> valueString;
    value = test->runJavaScriptAndWaitUntilFinished("document.getElementsByClassName('targeturl')[0].innerText;", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(value));
    g_assert_cmpstr(valueString.get(), ==, "http://127.0.0.1:2999/");

    value = test->runJavaScriptAndWaitUntilFinished("document.getElementsByTagName('input')[0].onclick.toString();", &error.outPtr());
    g_assert_nonnull(value);
    g_assert_no_error(error.get());
    valueString.reset(WebViewTest::javascriptResultToCString(value));
    g_assert_nonnull(g_strrstr(valueString.get(), "window.open('Main.html?ws=' + window.location.host + '/socket/1/1/WebPage', '_blank', 'location=no,menubar=no,status=no,toolbar=no');"));
}

const SocketConnection::MessageHandlers InspectorServerTest::s_observerMessageHandlers = {
    { "DidClose"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { { },
        [](SocketConnection&, GVariant*, gpointer) {
            g_printerr("Inspector server closed the connection\n");
            g_assert_not_reached();
        } }
    },
    { "DidStartAutomationSession"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { "(ss)"_s,
        [](SocketConnection&, GVariant*, gpointer userData) {
            auto* test = static_cast<InspectorServerTest*>(userData);
            test->m_automationSessionStarted = true;
            g_main_loop_quit(test->m_mainLoop);
        } }
    },
    { "SetTargetList"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { "(ta(tsssb))"_s,
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto* test = static_cast<InspectorServerTest*>(userData);
            guint64 connectionID;
            GRefPtr<GVariant> targetList;
            g_variant_get(parameters, "(t@a(tsssb))", &connectionID, &targetList.outPtr());
            if (g_variant_n_children(targetList.get())) {
                if (!test->m_observedTargetListConnectionID) {
                    test->m_observedTargetListConnectionID = connectionID;
                    g_main_loop_quit(test->m_mainLoop);
                }
                return;
            }
            if (connectionID != test->m_expectedClosedConnectionID)
                return;
            test->m_expectedClosedConnectionID = 0;
            g_main_loop_quit(test->m_mainLoop);
        } }
    }
};

static void testInspectorServerInvalidMessages(InspectorServerTest* test, gconstpointer)
{
    static const uint64_t invalidConnectionID = 0xdeadbeef;

    test->connectAsInspectorClient();
    test->m_clientConnection->sendMessage("Setup"_s, g_variant_new("(tt)", invalidConnectionID, static_cast<uint64_t>(1)));
    test->m_clientConnection->sendMessage("SendMessageToBackend"_s, g_variant_new("(tts)", invalidConnectionID, static_cast<uint64_t>(1), "{}"));
    test->m_clientConnection->sendMessage("FrontendDidClose"_s, g_variant_new("(tt)", invalidConnectionID, static_cast<uint64_t>(1)));

    test->setupInspectorClientAndWaitForTarget();
    test->m_clientConnection->sendMessage("Setup"_s, g_variant_new("(tt)", invalidConnectionID, test->m_targetID));
    test->m_clientConnection->sendMessage("SendMessageToBackend"_s, g_variant_new("(tts)", invalidConnectionID, test->m_targetID, "{}"));
    test->m_clientConnection->sendMessage("FrontendDidClose"_s, g_variant_new("(tt)", invalidConnectionID, test->m_targetID));

    test->m_clientConnection->sendMessage("Setup"_s, g_variant_new("(tt)", test->m_connectionID, test->m_targetID));
    test->m_clientConnection->sendMessage("SendMessageToBackend"_s, g_variant_new("(tts)", test->m_connectionID, test->m_targetID, "{\"id\":4242,\"method\":\"Test.invalidMethod\"}"));
    test->waitForFrontendMessage("\"id\":4242");
}

static void testInspectorServerConnectionWithMultipleRoles(InspectorServerTest* test, gconstpointer)
{
    static const uint64_t fakeTargetID = 0xfeedface;

    test->connectAsInspectorClient();
    test->setupInspectorClientAndWaitForTarget();

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(tsssb)"));
    g_variant_builder_add(&builder, "(tsssb)", fakeTargetID, "WebPage", "Fake", "about:blank", FALSE);
    test->m_clientConnection->sendMessage("SetTargetList"_s, g_variant_new("(@a(tsssb)b)", g_variant_builder_end(&builder), FALSE));
    test->waitForTargetList(fakeTargetID);
    uint64_t ownConnectionID = test->m_expectedTargetConnectionID;
    g_assert_cmpuint(ownConnectionID, !=, test->m_connectionID);

    test->m_clientConnection->sendMessage("Setup"_s, g_variant_new("(tt)", ownConnectionID, fakeTargetID));
    test->m_clientConnection->sendMessage("Setup"_s, g_variant_new("(tt)", test->m_connectionID, test->m_targetID));
    test->m_clientConnection->sendMessage("SendMessageToBackend"_s, g_variant_new("(tts)", test->m_connectionID, test->m_targetID, "{\"id\":4343,\"method\":\"Test.invalidMethod\"}"));
    test->waitForFrontendMessage("\"id\":4343");

    test->connectObserverAndWaitForAutomationSession();
    test->m_clientConnection->close();
    test->m_clientConnection = nullptr;
    test->waitForConnectionClosed(ownConnectionID);

    test->m_targetID = 0;
    test->connectAsInspectorClient();
    test->setupInspectorClientAndWaitForTarget();
    g_assert_cmpuint(test->m_connectionID, !=, ownConnectionID);

    test->m_clientConnection->sendMessage("Setup"_s, g_variant_new("(tt)", ownConnectionID, fakeTargetID));
    test->m_clientConnection->sendMessage("SendMessageToBackend"_s, g_variant_new("(tts)", ownConnectionID, fakeTargetID, "{}"));
    test->m_clientConnection->sendMessage("FrontendDidClose"_s, g_variant_new("(tt)", ownConnectionID, fakeTargetID));

    test->m_clientConnection->sendMessage("Setup"_s, g_variant_new("(tt)", test->m_connectionID, test->m_targetID));
    test->m_clientConnection->sendMessage("SendMessageToBackend"_s, g_variant_new("(tts)", test->m_connectionID, test->m_targetID, "{\"id\":4444,\"method\":\"Test.invalidMethod\"}"));
    test->waitForFrontendMessage("\"id\":4444");
}

const SocketConnection::MessageHandlers InspectorServerTest::s_secondClientMessageHandlers = {
    { "DidClose"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { { },
        [](SocketConnection&, GVariant*, gpointer) { } }
    },
    { "DidSetupInspectorClient"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { "(ay)"_s,
        [](SocketConnection&, GVariant*, gpointer userData) {
            auto* test = static_cast<InspectorServerTest*>(userData);
            test->m_secondClientWasAccepted = true;
            g_main_loop_quit(test->m_mainLoop);
        } }
    },
    { "DidStartAutomationSession"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { "(ss)"_s,
        [](SocketConnection&, GVariant*, gpointer userData) {
            auto* test = static_cast<InspectorServerTest*>(userData);
            test->m_secondClientWasAccepted = true;
            g_main_loop_quit(test->m_mainLoop);
        } }
    }
};

static GVariant* fakeTargetList(gboolean remoteAutomationEnabled)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(tsssb)"));
    g_variant_builder_add(&builder, "(tsssb)", static_cast<uint64_t>(1), "WebPage", "Test", "about:blank", FALSE);
    return g_variant_new("(@a(tsssb)b)", g_variant_builder_end(&builder), remoteAutomationEnabled);
}

static void testInspectorServerSecondClient(InspectorServerTest* test, gconstpointer)
{
    test->connectAsInspectorClient();
    test->setupInspectorClientAndWaitForTarget();
    test->m_clientConnection->sendMessage("Setup"_s, g_variant_new("(tt)", test->m_connectionID, test->m_targetID));
    test->m_clientConnection->sendMessage("SendMessageToBackend"_s, g_variant_new("(tts)", test->m_connectionID, test->m_targetID, "{\"id\":4242,\"method\":\"Test.invalidMethod\"}"));
    test->waitForFrontendMessage("\"id\":4242");

    auto secondClient = test->connectToServer(InspectorServerTest::s_secondClientMessageHandlers);
    secondClient->sendMessage("SetupInspectorClient"_s, g_variant_new("(@ay)", g_variant_new_bytestring("")));
    secondClient->sendMessage("SendMessageToBackend"_s, g_variant_new("(tts)", test->m_connectionID, test->m_targetID, "{\"id\":4141,\"method\":\"Test.invalidMethod\"}"));
    secondClient->sendMessage("FrontendDidClose"_s, g_variant_new("(tt)", test->m_connectionID, test->m_targetID));
    secondClient->sendMessage("SetTargetList"_s, fakeTargetList(FALSE));

    while (!test->m_otherConnectionID && !test->m_secondClientWasAccepted)
        g_main_loop_run(test->m_mainLoop);
    secondClient->close();
    g_assert_false(test->m_secondClientWasAccepted);

    test->m_unexpectedFrontendMessage = "\"id\":4141";
    test->m_clientConnection->sendMessage("SendMessageToBackend"_s, g_variant_new("(tts)", test->m_connectionID, test->m_targetID, "{\"id\":4343,\"method\":\"Test.invalidMethod\"}"));
    test->waitForFrontendMessage("\"id\":4343");
    g_assert_false(test->m_receivedUnexpectedFrontendMessage);
}

static void testInspectorServerSecondAutomationConnection(InspectorServerTest* test, gconstpointer)
{
    test->connectObserverAndWaitForAutomationSession();

    auto secondConnection = test->connectToServer(InspectorServerTest::s_secondClientMessageHandlers);
    secondConnection->sendMessage("StartAutomationSession"_s, g_variant_new("(s@a{sv})", "second", g_variant_new_array(G_VARIANT_TYPE("{sv}"), nullptr, 0)));
    secondConnection->sendMessage("SetTargetList"_s, fakeTargetList(TRUE));

    while (!test->m_observedTargetListConnectionID && !test->m_secondClientWasAccepted)
        g_main_loop_run(test->m_mainLoop);
    secondConnection->close();
    g_assert_false(test->m_secondClientWasAccepted);
}

void beforeAll()
{
    // Terminate the server process if any test fails.
    signal(SIGABRT, +[](int) {
        if (gActiveServerPid > 0)
            kill(gActiveServerPid, SIGTERM);
    });

    InspectorServerTest::add("WebKitWebInspectorServer", "test-page-list", testInspectorServerPageList);
    InspectorServerTest::add("WebKitWebInspectorServer", "invalid-messages", testInspectorServerInvalidMessages);
    InspectorServerTest::add("WebKitWebInspectorServer", "connection-with-multiple-roles", testInspectorServerConnectionWithMultipleRoles);
    InspectorServerTest::add("WebKitWebInspectorServer", "second-client", testInspectorServerSecondClient);
    InspectorServerTest::add("WebKitWebInspectorServer", "second-automation-connection", testInspectorServerSecondAutomationConnection);
    InspectorHTTPServerTest::add("WebKitWebInspectorServer", "http-test-page-list", testInspectorHTTPServerPageList);
}

void afterAll()
{
}
