/*
 * Copyright (C) 2017 Igalia S.L.
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

#include "TestMain.h"
#include <gio/gio.h>
#include <wtf/UUID.h>
#include <wtf/glib/SocketConnection.h>
#include <wtf/text/StringBuilder.h>

class AutomationTest: public Test {
public:
    MAKE_GLIB_TEST_FIXTURE(AutomationTest);

    static const SocketConnection::MessageHandlers s_messageHandlers;

    AutomationTest()
        : m_mainLoop(adoptGRef(g_main_loop_new(nullptr, TRUE)))
    {
        GRefPtr<GSocketClient> socketClient = adoptGRef(g_socket_client_new());
        g_socket_client_connect_to_host_async(socketClient.get(), "127.0.0.1:2229", 0, nullptr, [](GObject* client, GAsyncResult* result, gpointer userData) {
            GRefPtr<GSocketConnection> connection = adoptGRef(g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(client), result, nullptr));
            static_cast<AutomationTest*>(userData)->setConnection(SocketConnection::create(WTFMove(connection), s_messageHandlers, userData));
        }, this);
        g_main_loop_run(m_mainLoop.get());
    }

    ~AutomationTest()
    {
    }

    struct Target {
        Target() = default;
        Target(guint64 id, CString name, bool isPaired)
            : id(id)
            , name(name)
            , isPaired(isPaired)
        {
        }

        guint64 id { 0 };
        CString name;
        bool isPaired { false };
    };

    void setConnection(Ref<SocketConnection>&& connection)
    {
        g_assert_true(connection.ptr());
        m_connection = WTFMove(connection);
        g_main_loop_quit(m_mainLoop.get());
    }

    void setTarget(guint64 connectionID, Target&& target)
    {
        bool newConnection = !m_connectionID;
        bool wasPaired = m_target.isPaired;
        m_connectionID = connectionID;
        m_target = WTFMove(target);
        if (newConnection || (!wasPaired && m_target.isPaired))
            g_main_loop_quit(m_mainLoop.get());
    }

    void receivedMessage(guint64 connectionID, guint64 targetID, const char* message)
    {
        g_assert_cmpuint(connectionID, ==, m_connectionID);
        g_assert_cmpuint(targetID, ==, m_target.id);
        m_message = message;
        g_main_loop_quit(m_mainLoop.get());
    }

    void sendCommandToBackend(const String& command, const String& parameters = String())
    {
        static long sequenceID = 0;
        StringBuilder messageBuilder;
        messageBuilder.appendLiteral("{\"id\":");
        messageBuilder.appendNumber(++sequenceID);
        messageBuilder.appendLiteral(",\"method\":\"Automation.");
        messageBuilder.append(command);
        messageBuilder.append('"');
        if (!parameters.isNull()) {
            messageBuilder.appendLiteral(",\"params\":");
            messageBuilder.append(parameters);
        }
        messageBuilder.append('}');
        m_connection->sendMessage("SendMessageToBackend", g_variant_new("(tts)", m_connectionID, m_target.id, messageBuilder.toString().utf8().data()));
    }

    static WebKitWebView* createWebViewCallback(WebKitAutomationSession* session, AutomationTest* test)
    {
        test->m_createWebViewWasCalled = true;
        return test->m_webViewForAutomation;
    }

    static WebKitWebView* createWebViewInWindowCallback(WebKitAutomationSession* session, AutomationTest* test)
    {
        test->m_createWebViewInWindowWasCalled = true;
        return test->m_webViewForAutomation;
    }

    static WebKitWebView* createWebViewInTabCallback(WebKitAutomationSession* session, AutomationTest* test)
    {
        test->m_createWebViewInTabWasCalled = true;
        return test->m_webViewForAutomation;
    }

    void automationStarted(WebKitAutomationSession* session)
    {
        m_session = session;
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_session));
        g_assert_null(webkit_automation_session_get_application_info(session));
        WebKitApplicationInfo* info = webkit_application_info_new();
        webkit_application_info_set_name(info, "AutomationTestBrowser");
        webkit_application_info_set_version(info, WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION, WEBKIT_MICRO_VERSION);
        webkit_automation_session_set_application_info(session, info);
        webkit_application_info_unref(info);
        g_assert_true(webkit_automation_session_get_application_info(session) == info);
    }

    static void automationStartedCallback(WebKitWebContext* webContext, WebKitAutomationSession* session, AutomationTest* test)
    {
        g_assert_true(webContext == test->m_webContext.get());
        g_assert_true(WEBKIT_IS_AUTOMATION_SESSION(session));
        test->automationStarted(session);
    }

    static GUniquePtr<char> toVersionString(unsigned major, unsigned minor, unsigned micro)
    {
        if (!micro && !minor)
            return GUniquePtr<char>(g_strdup_printf("%u", major));

        if (!micro)
            return GUniquePtr<char>(g_strdup_printf("%u.%u", major, minor));

        return GUniquePtr<char>(g_strdup_printf("%u.%u.%u", major, minor, micro));
    }

    void didStartAutomationSession(GVariant* capabilities)
    {
        if (!m_session)
            return;

        g_assert_nonnull(capabilities);
        const char* browserName;
        const char* browserVersion;
        g_variant_get(capabilities, "(&s&s)", &browserName, &browserVersion);
        g_assert_cmpstr(browserName, ==, "AutomationTestBrowser");
        GUniquePtr<char> versionString = toVersionString(WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION, WEBKIT_MICRO_VERSION);
        g_assert_cmpstr(browserVersion, ==, versionString.get());
    }

    WebKitAutomationSession* requestSession(const char* sessionID)
    {
        auto signalID = g_signal_connect(m_webContext.get(), "automation-started", G_CALLBACK(automationStartedCallback), this);
        m_connection->sendMessage("StartAutomationSession", g_variant_new("(sa{sv})", sessionID, nullptr));
        auto timeoutID = g_timeout_add(1000, [](gpointer userData) -> gboolean {
            g_main_loop_quit(static_cast<GMainLoop*>(userData));
            return G_SOURCE_REMOVE;
        }, m_mainLoop.get());
        g_main_loop_run(m_mainLoop.get());
        if (!m_connectionID)
            m_session = nullptr;
        if (m_session && m_connectionID)
            g_source_remove(timeoutID);
        g_signal_handler_disconnect(m_webContext.get(), signalID);
        return m_session;
    }

    void setupIfNeeded()
    {
        if (m_target.isPaired)
            return;
        g_assert_cmpuint(m_target.id, !=, 0);
        m_connection->sendMessage("Setup", g_variant_new("(tt)", m_connectionID, m_target.id));
        g_main_loop_run(m_mainLoop.get());
        g_assert_true(m_target.isPaired);
    }

    bool createTopLevelBrowsingContext(WebKitWebView* webView)
    {
        setupIfNeeded();
        m_webViewForAutomation = webView;
        m_createWebViewWasCalled = false;
        m_message = CString();
        auto signalID = g_signal_connect(m_session, "create-web-view", G_CALLBACK(createWebViewCallback), this);
        sendCommandToBackend("createBrowsingContext");
        g_main_loop_run(m_mainLoop.get());
        g_signal_handler_disconnect(m_session, signalID);
        g_assert_true(m_createWebViewWasCalled);
        g_assert_false(m_message.isNull());
        m_webViewForAutomation = nullptr;

        if (strstr(m_message.data(), "The remote session failed to create a new browsing context"))
            return false;
        if (strstr(m_message.data(), "handle"))
            return true;
        return false;
    }

    bool createNewWindow(WebKitWebView* webView)
    {
        m_webViewForAutomation = webView;
        m_createWebViewInWindowWasCalled = false;
        m_message = { };
        auto signalID = g_signal_connect(m_session, "create-web-view::window", G_CALLBACK(createWebViewInWindowCallback), this);
        sendCommandToBackend("createBrowsingContext", "{\"presentationHint\":\"Window\"}");
        g_main_loop_run(m_mainLoop.get());
        g_signal_handler_disconnect(m_session, signalID);
        g_assert_true(m_createWebViewInWindowWasCalled);
        g_assert_false(m_message.isNull());
        m_webViewForAutomation = nullptr;

        if (strstr(m_message.data(), "\"presentation\":\"Window\""))
            return true;
        return false;
    }

    bool createNewTab(WebKitWebView* webView)
    {
        m_webViewForAutomation = webView;
        m_createWebViewInTabWasCalled = false;
        m_message = { };
        auto signalID = g_signal_connect(m_session, "create-web-view::tab", G_CALLBACK(createWebViewInTabCallback), this);
        sendCommandToBackend("createBrowsingContext", "{\"presentationHint\":\"Tab\"}");
        g_main_loop_run(m_mainLoop.get());
        g_signal_handler_disconnect(m_session, signalID);
        g_assert_true(m_createWebViewInTabWasCalled);
        g_assert_false(m_message.isNull());
        m_webViewForAutomation = nullptr;

        if (strstr(m_message.data(), "\"presentation\":\"Tab\""))
            return true;
        return false;
    }

    GRefPtr<GMainLoop> m_mainLoop;
    RefPtr<SocketConnection> m_connection;
    WebKitAutomationSession* m_session;
    guint64 m_connectionID { 0 };
    Target m_target;

    WebKitWebView* m_webViewForAutomation { nullptr };
    bool m_createWebViewWasCalled { false };
    bool m_createWebViewInWindowWasCalled { false };
    bool m_createWebViewInTabWasCalled { false };
    CString m_message;
};

const SocketConnection::MessageHandlers AutomationTest::s_messageHandlers = {
    { "DidClose", std::pair<CString, SocketConnection::MessageCallback> { { },
        [](SocketConnection&, GVariant*, gpointer userData) {
            auto& test = *static_cast<AutomationTest*>(userData);
            test.m_connection = nullptr;
        }}
    },
    { "DidStartAutomationSession", std::pair<CString, SocketConnection::MessageCallback> { "(ss)",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& test = *static_cast<AutomationTest*>(userData);
            test.didStartAutomationSession(parameters);
        }}
    },
    { "SetTargetList", std::pair<CString, SocketConnection::MessageCallback> { "(ta(tsssb))",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& test = *static_cast<AutomationTest*>(userData);
            guint64 connectionID;
            GUniqueOutPtr<GVariantIter> iter;
            g_variant_get(parameters, "(ta(tsssb))", &connectionID, &iter.outPtr());
            guint64 targetID;
            const char* type;
            const char* name;
            const char* dummy;
            gboolean isPaired;
            while (g_variant_iter_loop(iter.get(), "(t&s&s&sb)", &targetID, &type, &name, &dummy, &isPaired)) {
                if (!g_strcmp0(type, "Automation")) {
                    test.setTarget(connectionID, Target(targetID, name, isPaired));
                    break;
                }
            }
        }}
    },
    { "SendMessageToFrontend", std::pair<CString, SocketConnection::MessageCallback> { "(tts)",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& test = *static_cast<AutomationTest*>(userData);
            guint64 connectionID, targetID;
            const char* message;
            g_variant_get(parameters, "(tt&s)", &connectionID, &targetID, &message);
            test.receivedMessage(connectionID, targetID, message);
        }}
    }
};

static void testAutomationSessionRequestSession(AutomationTest* test, gconstpointer)
{
    String sessionID = createCanonicalUUIDString();
    // WebKitAutomationSession::automation-started is never emitted if automation is not enabled.
    g_assert_false(webkit_web_context_is_automation_allowed(test->m_webContext.get()));
    auto* session = test->requestSession(sessionID.utf8().data());
    g_assert_null(session);

    webkit_web_context_set_automation_allowed(test->m_webContext.get(), TRUE);
    g_assert_true(webkit_web_context_is_automation_allowed(test->m_webContext.get()));

    // There can't be more than one context with automation enabled
    GRefPtr<WebKitWebContext> otherContext = adoptGRef(webkit_web_context_new());
    Test::removeLogFatalFlag(G_LOG_LEVEL_WARNING);
    webkit_web_context_set_automation_allowed(otherContext.get(), TRUE);
    Test::addLogFatalFlag(G_LOG_LEVEL_WARNING);
    g_assert_false(webkit_web_context_is_automation_allowed(otherContext.get()));

    session = test->requestSession(sessionID.utf8().data());
    g_assert_cmpstr(webkit_automation_session_get_id(session), ==, sessionID.utf8().data());
    g_assert_cmpuint(test->m_target.id, >, 0);
    ASSERT_CMP_CSTRING(test->m_target.name, ==, sessionID.utf8());
    g_assert_false(test->m_target.isPaired);

    // Will fail to create a browsing context when not creating a web view (or not handling the signal).
    g_assert_false(test->createTopLevelBrowsingContext(nullptr));

    // Will also fail if the web view is not controlled by automation.
    auto webView = Test::adoptView(Test::createWebView(test->m_webContext.get()));
    g_assert_false(webkit_web_view_is_controlled_by_automation(webView.get()));
    g_assert_false(test->createTopLevelBrowsingContext(webView.get()));

    // And will work with a proper web view.
    webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
        "backend", Test::createWebViewBackend(),
#endif
        "web-context", test->m_webContext.get(),
        "is-controlled-by-automation", TRUE,
        nullptr));
    g_assert_true(webkit_web_view_is_controlled_by_automation(webView.get()));
    g_assert_cmpuint(webkit_web_view_get_automation_presentation_type(webView.get()), ==, WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_WINDOW);
    g_assert_true(test->createTopLevelBrowsingContext(webView.get()));

    auto newWebViewInWindow = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
        "backend", Test::createWebViewBackend(),
#endif
        "web-context", test->m_webContext.get(),
        "is-controlled-by-automation", TRUE,
        nullptr));
    g_assert_true(webkit_web_view_is_controlled_by_automation(newWebViewInWindow.get()));
    g_assert_cmpuint(webkit_web_view_get_automation_presentation_type(newWebViewInWindow.get()), ==, WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_WINDOW);
    g_assert_true(test->createNewWindow(newWebViewInWindow.get()));

    auto newWebViewInTab = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
        "backend", Test::createWebViewBackend(),
#endif
        "web-context", test->m_webContext.get(),
        "is-controlled-by-automation", TRUE,
        "automation-presentation-type", WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_TAB,
        nullptr));
    g_assert_true(webkit_web_view_is_controlled_by_automation(newWebViewInTab.get()));
    g_assert_cmpuint(webkit_web_view_get_automation_presentation_type(newWebViewInTab.get()), ==, WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_TAB);
    g_assert_true(test->createNewTab(newWebViewInTab.get()));

    webkit_web_context_set_automation_allowed(test->m_webContext.get(), FALSE);
}

static void testAutomationSessionApplicationInfo(Test* test, gconstpointer)
{
    WebKitApplicationInfo* info = webkit_application_info_new();
    g_assert_cmpstr(webkit_application_info_get_name(info), ==, g_get_prgname());
    webkit_application_info_set_name(info, "WebKitGTKBrowser");
    g_assert_cmpstr(webkit_application_info_get_name(info), ==, "WebKitGTKBrowser");
    webkit_application_info_set_name(info, nullptr);
    g_assert_cmpstr(webkit_application_info_get_name(info), ==, g_get_prgname());

    guint64 major, minor, micro;
    webkit_application_info_get_version(info, &major, nullptr, nullptr);
    g_assert_cmpuint(major, ==, 0);
    webkit_application_info_set_version(info, 1, 2, 3);
    webkit_application_info_get_version(info, &major, &minor, &micro);
    g_assert_cmpuint(major, ==, 1);
    g_assert_cmpuint(minor, ==, 2);
    g_assert_cmpuint(micro, ==, 3);

    webkit_application_info_unref(info);
}


void beforeAll()
{
    g_setenv("WEBKIT_INSPECTOR_SERVER", "127.0.0.1:2229", TRUE);

    AutomationTest::add("WebKitAutomationSession", "request-session", testAutomationSessionRequestSession);
    Test::add("WebKitAutomationSession", "application-info", testAutomationSessionApplicationInfo);
}

void afterAll()
{
    g_unsetenv("WEBKIT_INSPECTOR_SERVER");
}
