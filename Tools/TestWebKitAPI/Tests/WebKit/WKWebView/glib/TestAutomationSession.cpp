/*
 * Copyright (C) 2017 Igalia S.L.
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

#include "TestMain.h"
#include "WebKitTestServer.h"
#include <gio/gio.h>
#include <wtf/Function.h>
#include <wtf/JSONValues.h>
#include <wtf/UUID.h>
#include <wtf/glib/GLibExtras.h>
#include <wtf/glib/SocketConnection.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/UTF8CStringView.h>

static WebKitTestServer* kServer;

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
            static_cast<AutomationTest*>(userData)->setConnection(SocketConnection::create(WTF::move(connection), s_messageHandlers, userData));
        }, this);
        g_main_loop_run(m_mainLoop.get());
    }

    struct Target {
        Target() = default;
        Target(guint64 id, UTF8CString&& name, bool isPaired)
            : id(id)
            , name(WTF::move(name))
            , isPaired(isPaired)
        {
        }

        guint64 id { 0 };
        UTF8CString name;
        bool isPaired { false };
    };

    void setConnection(Ref<SocketConnection>&& connection)
    {
        m_connection = WTF::move(connection);
        g_main_loop_quit(m_mainLoop.get());
    }

    void setTarget(guint64 connectionID, Target&& target)
    {
        bool newConnection = !m_connectionID;
        bool wasPaired = m_target.isPaired;
        m_connectionID = connectionID;
        m_target = WTF::move(target);
        if (newConnection || (!wasPaired && m_target.isPaired))
            g_main_loop_quit(m_mainLoop.get());
    }

    void receivedMessage(guint64 connectionID, guint64 targetID, const char* message)
    {
        g_assert_cmpuint(connectionID, ==, m_connectionID);
        g_assert_cmpuint(targetID, ==, m_target.id);
        m_message = UTF8CString::unsafeFromUTF8(message);
        m_messages.append(String::fromUTF8(message));
        g_main_loop_quit(m_mainLoop.get());
    }

    // Runs the main loop until the condition holds, collecting every message received meanwhile in m_messages.
    // Fails the test if that takes longer than the overall timeout.
    void waitUntil(NOESCAPE const Function<bool()>& condition)
    {
        static constexpr gint64 timeoutInMicroseconds = 30 * G_USEC_PER_SEC;
        gint64 deadline = g_get_monotonic_time() + timeoutInMicroseconds;
        // Wake the loop up regularly: not everything the conditions observe arrives as a message.
        auto tickID = g_timeout_add(20, [](gpointer userData) -> gboolean {
            g_main_loop_quit(static_cast<GMainLoop*>(userData));
            return G_SOURCE_CONTINUE;
        }, m_mainLoop.get());
        while (!condition()) {
            g_assert_cmpint(g_get_monotonic_time(), <, deadline);
            g_main_loop_run(m_mainLoop.get());
        }
        g_source_remove(tickID);
    }

    RefPtr<JSON::Object> responseForCommand(long commandID) const
    {
        for (auto& message : m_messages) {
            auto value = JSON::Value::parseJSON(message);
            auto object = value ? value->asObject() : nullptr;
            if (object && object->getInteger("id"_s) == commandID)
                return object;
        }
        return nullptr;
    }

    long sendNavigateCommand(const String& pageHandle, const String& url, ASCIILiteral pageLoadStrategy = "Normal"_s)
    {
        // The page load timeout bounds the checks should a command never complete.
        return sendCommandToBackend("navigateBrowsingContext"_s, makeString("{\"handle\":\""_s, pageHandle, "\",\"url\":\""_s, url, "\",\"pageLoadStrategy\":\""_s, pageLoadStrategy, "\",\"pageLoadTimeout\":10000}"_s));
    }

    RefPtr<JSON::Object> waitForResponse(long commandID)
    {
        RefPtr<JSON::Object> response;
        waitUntil([&] {
            response = responseForCommand(commandID);
            return !!response;
        });
        return response;
    }

    RefPtr<JSON::Object> navigateAndWaitForResponse(const String& pageHandle, const UTF8CString& url)
    {
        return waitForResponse(sendNavigateCommand(pageHandle, String::fromUTF8(url.span())));
    }

    long sendCommandToBackend(const String& command, const String& parameters = String())
    {
        static long sequenceID = 0;
        StringBuilder messageBuilder;
        messageBuilder.append("{\"id\":"_s, ++sequenceID, ",\"method\":\"Automation."_s, command, '"');
        if (!parameters.isNull())
            messageBuilder.append(",\"params\":"_s, parameters);
        messageBuilder.append('}');
        m_connection->sendMessage("SendMessageToBackend"_s, gVariantNew("(tts)", m_connectionID, m_target.id, messageBuilder.toString().utf8()));
        return sequenceID;
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

    WebKitAutomationSession* requestSession(UTF8CStringView sessionID)
    {
        auto signalID = g_signal_connect(m_webContext.get(), "automation-started", G_CALLBACK(automationStartedCallback), this);
        m_connection->sendMessage("StartAutomationSession"_s, gVariantNew("(sa{sv})", sessionID, nullptr));
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
        m_connection->sendMessage("Setup"_s, g_variant_new("(tt)", m_connectionID, m_target.id));
        g_main_loop_run(m_mainLoop.get());
        g_assert_true(m_target.isPaired);
    }

    bool createTopLevelBrowsingContext(WebKitWebView* webView)
    {
        setupIfNeeded();
        m_webViewForAutomation = webView;
        m_createWebViewWasCalled = false;
        m_message = { };
        auto signalID = g_signal_connect(m_session, "create-web-view", G_CALLBACK(createWebViewCallback), this);
        sendCommandToBackend("createBrowsingContext"_s);
        g_main_loop_run(m_mainLoop.get());
        g_signal_handler_disconnect(m_session, signalID);
        g_assert_true(m_createWebViewWasCalled);
        g_assert_false(m_message.isNull());
        m_webViewForAutomation = nullptr;

        if (contains(m_message.span(), u8"The remote session failed to create a new browsing context"_span))
            return false;
        if (contains(m_message.span(), u8"handle"_span))
            return true;
        return false;
    }

    bool createNewWindow(WebKitWebView* webView)
    {
        m_webViewForAutomation = webView;
        m_createWebViewInWindowWasCalled = false;
        m_message = { };
        auto signalID = g_signal_connect(m_session, "create-web-view::window", G_CALLBACK(createWebViewInWindowCallback), this);
        sendCommandToBackend("createBrowsingContext"_s, "{\"presentationHint\":\"Window\"}"_s);
        g_main_loop_run(m_mainLoop.get());
        g_signal_handler_disconnect(m_session, signalID);
        g_assert_true(m_createWebViewInWindowWasCalled);
        g_assert_false(m_message.isNull());
        m_webViewForAutomation = nullptr;

        if (contains(m_message.span(), u8"\"presentation\":\"Window\""_span))
            return true;
        return false;
    }

    bool createNewTab(WebKitWebView* webView)
    {
        m_webViewForAutomation = webView;
        m_createWebViewInTabWasCalled = false;
        m_message = { };
        auto signalID = g_signal_connect(m_session, "create-web-view::tab", G_CALLBACK(createWebViewInTabCallback), this);
        sendCommandToBackend("createBrowsingContext"_s, "{\"presentationHint\":\"Tab\"}"_s);
        g_main_loop_run(m_mainLoop.get());
        g_signal_handler_disconnect(m_session, signalID);
        g_assert_true(m_createWebViewInTabWasCalled);
        g_assert_false(m_message.isNull());
        m_webViewForAutomation = nullptr;

        if (contains(m_message.span(), u8"\"presentation\":\"Tab\""_span))
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
    UTF8CString m_message;
    Vector<String> m_messages;
};

const SocketConnection::MessageHandlers AutomationTest::s_messageHandlers = {
    { "DidClose"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { { },
        [](SocketConnection&, GVariant*, gpointer userData) {
            auto& test = *static_cast<AutomationTest*>(userData);
            test.m_connection = nullptr;
        }}
    },
    { "DidStartAutomationSession"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { "(ss)"_s,
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& test = *static_cast<AutomationTest*>(userData);
            test.didStartAutomationSession(parameters);
        }}
    },
    { "SetTargetList"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { "(ta(tsssb))"_s,
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
                    test.setTarget(connectionID, Target(targetID, UTF8CString::unsafeFromUTF8(name), isPaired));
                    break;
                }
            }
        }}
    },
    { "SendMessageToFrontend"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { "(tts)"_s,
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& test = *static_cast<AutomationTest*>(userData);
            guint64 connectionID, targetID;
            const char* message;
            g_variant_get(parameters, "(tt&s)", &connectionID, &targetID, &message);
            test.receivedMessage(connectionID, targetID, message);
        }}
    }
};

static bool s_slowScriptWasServed;
static bool s_stalledLoadWasRequested;
static bool s_slowReplacementWasServed;
static GRefPtr<SoupServerMessage> s_heldReplacementRequest;

// Answers the /slow-replacement request the server has been holding.
static void serveHeldReplacement()
{
    auto message = std::exchange(s_heldReplacementRequest, nullptr);
    g_assert_nonnull(message.get());
    soup_server_message_set_status(message.get(), SOUP_STATUS_OK, nullptr);
    soup_message_headers_set_content_type(soup_server_message_get_response_headers(message.get()), "text/html", nullptr);
    auto* responseBody = soup_server_message_get_response_body(message.get());
    static const char* html = "<p>replacement</p>";
    soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, html, strlen(html));
    soup_message_body_complete(responseBody);
    s_slowReplacementWasServed = true;
    soup_server_unpause_message(kServer->soupServer(), message.get());
}
static GRefPtr<WebKitPolicyDecision> s_heldPolicyDecision;

// Keeps the navigation policy decision of a request for /held without deciding it, so that the navigation stays
// in its policy check until something else ends it.
static gboolean holdPolicyDecisionForHeldRequest(WebKitWebView*, WebKitPolicyDecision* decision, WebKitPolicyDecisionType type, gpointer)
{
    if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
        return FALSE;
    auto* request = webkit_navigation_action_get_request(webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(decision)));
    if (!g_str_has_suffix(webkit_uri_request_get_uri(request), "/held"))
        return FALSE;
    s_heldPolicyDecision = decision;
    return TRUE;
}
// A port nothing listens on, for navigations that must fail: a blocked port would be refused before any load starts.
static guint16 s_unreachablePort;

// Only one automation session can be established per test process, so this runs inside the session of
// testAutomationSessionRequestSession() instead of being a test of its own.
static void checkNavigationCommandsAwaitTheirOwnNavigation(AutomationTest* test)
{
    // A browser usually starts a load in the view it hands to the session, which is still in flight when the
    // first navigate command arrives. This one is never answered, so only a mistake can end the wait with it.
    // The server has received its request before the command is sent, so the load is provisional by then.
    auto webView = test->createWebView("is-controlled-by-automation", TRUE, nullptr);
    s_stalledLoadWasRequested = false;
    webkit_web_view_load_uri(webView.get(), kServer->getURIForPath("/stall").legacyCStringPointer());
    test->waitUntil([&] {
        return s_stalledLoadWasRequested;
    });
    g_assert_true(test->createTopLevelBrowsingContext(webView.get()));
    auto createResponse = JSON::Value::parseJSON(String::fromUTF8(test->m_message.span()));
    g_assert_nonnull(createResponse.get());
    auto createResult = createResponse->asObject()->getObject("result"_s);
    g_assert_nonnull(createResult.get());
    String pageHandle = createResult->getString("handle"_s);
    g_assert_false(pageHandle.isEmpty());

    // The command completes once the navigation it started has loaded, not when the earlier load ends.
    auto targetURI = kServer->getURIForPath("/target");
    auto response = test->navigateAndWaitForResponse(pageHandle, targetURI);
    g_assert_nonnull(response->getObject("result"_s).get());
    g_assert_false(webkit_web_view_is_loading(webView.get()));
    ASSERT_CMP_CSTRING(webkit_web_view_get_uri(webView.get()), ==, targetURI);

    // A same-document navigation that a script of the loading document performs does not complete the command
    // either: it is reported without a navigation, and the load the command waits for is still in progress.
    auto fragmentURI = kServer->getURIForPath("/fragment-while-loading");
    response = test->navigateAndWaitForResponse(pageHandle, fragmentURI);
    g_assert_nonnull(response->getObject("result"_s).get());
    g_assert_false(webkit_web_view_is_loading(webView.get()));
    ASSERT_CMP_CSTRING(webkit_web_view_get_uri(webView.get()), ==, makeString(String::fromUTF8(fragmentURI.span()), "#changed"_s).utf8());

    // A navigation the page starts while the awaited one still awaits its policy decision cancels that
    // decision: the awaited navigation is destroyed without reporting anything. The command completes with the
    // load of the navigation that replaced it, not before.
    s_heldPolicyDecision = nullptr;
    auto policySignalID = g_signal_connect(webView.get(), "decide-policy", G_CALLBACK(holdPolicyDecisionForHeldRequest), nullptr);
    auto heldCommandID = test->sendNavigateCommand(pageHandle, String::fromUTF8(kServer->getURIForPath("/held").span()));
    test->waitUntil([&] {
        return !!s_heldPolicyDecision;
    });
    auto replacementURI = kServer->getURIForPath("/replacement");
    webkit_web_view_evaluate_javascript(webView.get(), makeString("location.href = '"_s, String::fromUTF8(replacementURI.span()), '\'').utf8().legacyCStringPointer(), -1, nullptr, nullptr, nullptr, nullptr, nullptr);
    response = test->waitForResponse(heldCommandID);
    g_assert_nonnull(response->getObject("result"_s).get());
    g_assert_false(webkit_web_view_is_loading(webView.get()));
    ASSERT_CMP_CSTRING(webkit_web_view_get_uri(webView.get()), ==, replacementURI);

    // While the replacement is still loading, a same-document navigation of the old document is not the end of
    // the command: that is still the replacement's load. The server holds the replacement's response; its
    // request means the replacement has passed its policy check, which cancelled the held one.
    s_heldPolicyDecision = nullptr;
    s_slowReplacementWasServed = false;
    heldCommandID = test->sendNavigateCommand(pageHandle, String::fromUTF8(kServer->getURIForPath("/held").span()));
    test->waitUntil([&] {
        return !!s_heldPolicyDecision;
    });
    auto slowReplacementURI = kServer->getURIForPath("/slow-replacement");
    webkit_web_view_evaluate_javascript(webView.get(), makeString("location.href = '"_s, String::fromUTF8(slowReplacementURI.span()), '\'').utf8().legacyCStringPointer(), -1, nullptr, nullptr, nullptr, nullptr, nullptr);
    test->waitUntil([&] {
        return !!s_heldReplacementRequest;
    });
    // The script's result follows the notification its history change sends, so the session has seen that by
    // the time the script is done; a command sent then is answered after any response the change produced.
    bool historyChangeDone = false;
    webkit_web_view_evaluate_javascript(webView.get(), "history.pushState(null, '', '#old-document')", -1, nullptr, nullptr, nullptr, [](GObject*, GAsyncResult*, gpointer userData) {
        *static_cast<bool*>(userData) = true;
    }, &historyChangeDone);
    test->waitUntil([&] {
        return historyChangeDone;
    });
    auto barrierID = test->sendCommandToBackend("getBrowsingContexts"_s);
    test->waitUntil([&] {
        return !!test->responseForCommand(barrierID);
    });
    g_assert_null(test->responseForCommand(heldCommandID).get());
    serveHeldReplacement();
    response = test->waitForResponse(heldCommandID);
    g_assert_nonnull(response->getObject("result"_s).get());
    ASSERT_CMP_CSTRING(webkit_web_view_get_uri(webView.get()), ==, slowReplacementURI);
    g_signal_handler_disconnect(webView.get(), policySignalID);
    s_heldPolicyDecision = nullptr;

    // A navigate sent while the error page for a failed load is being loaded: that load may cancel the policy
    // check of the command's navigation, and has no navigation of its own. The command still completes.
    auto unreachableURI = makeString("http://127.0.0.1:"_s, s_unreachablePort, '/').utf8();
    response = test->navigateAndWaitForResponse(pageHandle, unreachableURI);
    g_assert_nonnull(response->getObject("result"_s).get());
    response = test->navigateAndWaitForResponse(pageHandle, targetURI);
    g_assert_nonnull(response->getObject("result"_s).get());

    // A wait with the eager strategy normally ends with the document load, which a navigation that failed before
    // committing never reaches: here the second command's navigation cancels the first one's provisional load.
    test->waitUntil([&] {
        return !webkit_web_view_is_loading(webView.get());
    });
    s_stalledLoadWasRequested = false;
    auto eagerCommandID = test->sendNavigateCommand(pageHandle, String::fromUTF8(kServer->getURIForPath("/stall").span()), "Eager"_s);
    test->waitUntil([&] {
        return s_stalledLoadWasRequested;
    });
    auto failingCommandID = test->sendNavigateCommand(pageHandle, String::fromUTF8(unreachableURI.span()));
    g_assert_nonnull(test->waitForResponse(failingCommandID)->getObject("result"_s).get());
    g_assert_nonnull(test->waitForResponse(eagerCommandID)->getObject("result"_s).get());

    // A wait that is not bound to a navigation (waitForNavigationToComplete, which WebDriver sends after Element
    // Click) ends with the notification its strategy specifies only: with the eager strategy, the document load
    // of the new page, not the end of the old page's load when the new navigation stops it.
    response = test->waitForResponse(test->sendNavigateCommand(pageHandle, String::fromUTF8(kServer->getURIForPath("/slow-image-page").span()), "Eager"_s));
    g_assert_nonnull(response->getObject("result"_s).get());
    g_assert_true(webkit_web_view_is_loading(webView.get()));
    s_slowScriptWasServed = false;
    webkit_web_view_evaluate_javascript(webView.get(), makeString("location.href = '"_s, String::fromUTF8(kServer->getURIForPath("/slow-script-page").span()), '\'').utf8().legacyCStringPointer(), -1, nullptr, nullptr, nullptr, nullptr, nullptr);
    auto waitCommandID = test->sendCommandToBackend("waitForNavigationToComplete"_s, makeString("{\"browsingContextHandle\":\""_s, pageHandle, "\",\"pageLoadStrategy\":\"Eager\",\"pageLoadTimeout\":10000}"_s));
    g_assert_nonnull(test->waitForResponse(waitCommandID)->getObject("result"_s).get());
    g_assert_true(s_slowScriptWasServed);

    // A fragment change the loading document makes to itself is reported with that document's navigation. It
    // does not complete the command either: the load, or the document load for the eager strategy, is not done.
    auto anchorURI = kServer->getURIForPath("/anchor-while-parsing");
    response = test->navigateAndWaitForResponse(pageHandle, anchorURI);
    g_assert_nonnull(response->getObject("result"_s).get());
    g_assert_false(webkit_web_view_is_loading(webView.get()));
    ASSERT_CMP_CSTRING(webkit_web_view_get_uri(webView.get()), ==, makeString(String::fromUTF8(anchorURI.span()), "#changed"_s).utf8());
    s_slowScriptWasServed = false;
    response = test->waitForResponse(test->sendNavigateCommand(pageHandle, String::fromUTF8(anchorURI.span()), "Eager"_s));
    g_assert_nonnull(response->getObject("result"_s).get());
    g_assert_true(s_slowScriptWasServed);
}

static void testAutomationSessionRequestSession(AutomationTest* test, gconstpointer)
{
    auto sessionID = createVersion4UUIDString().utf8();
    // WebKitAutomationSession::automation-started is never emitted if automation is not enabled.
    g_assert_false(webkit_web_context_is_automation_allowed(test->m_webContext.get()));
#if ENABLE(2022_GLIB_API)
    // Network session for automation is nullptr if automation is not enabled.
    g_assert_null(webkit_web_context_get_network_session_for_automation(test->m_webContext.get()));
#endif
    auto* session = test->requestSession(sessionID);
    g_assert_null(session);

    webkit_web_context_set_automation_allowed(test->m_webContext.get(), TRUE);
    g_assert_true(webkit_web_context_is_automation_allowed(test->m_webContext.get()));
#if ENABLE(2022_GLIB_API)
    auto* networkSession = webkit_web_context_get_network_session_for_automation(test->m_webContext.get());
    g_assert_nonnull(networkSession);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(networkSession));
    g_assert_true(webkit_network_session_is_ephemeral(networkSession));
    g_assert_false(networkSession == test->m_networkSession.get());
#endif

    // There can't be more than one context with automation enabled
    GRefPtr<WebKitWebContext> otherContext = adoptGRef(webkit_web_context_new());
    Test::removeLogFatalFlag(G_LOG_LEVEL_WARNING);
    webkit_web_context_set_automation_allowed(otherContext.get(), TRUE);
    Test::addLogFatalFlag(G_LOG_LEVEL_WARNING);
    g_assert_false(webkit_web_context_is_automation_allowed(otherContext.get()));

    session = test->requestSession(sessionID);
    ASSERT_CMP_CSTRING(webkit_automation_session_get_id(session), ==, sessionID);
    g_assert_cmpuint(test->m_target.id, >, 0);
    ASSERT_CMP_CSTRING(test->m_target.name, ==, sessionID);
    g_assert_false(test->m_target.isPaired);

    // Will fail to create a browsing context when not creating a web view (or not handling the signal).
    g_assert_false(test->createTopLevelBrowsingContext(nullptr));

    // Will also fail if the web view is not controlled by automation.
    auto webView = test->createWebView();
    g_assert_false(webkit_web_view_is_controlled_by_automation(webView.get()));
    g_assert_false(test->createTopLevelBrowsingContext(webView.get()));
#if ENABLE(2022_GLIB_API)
    g_assert_false(webkit_web_view_get_network_session(webView.get()) == networkSession);
#endif

    // And will work with a proper web view.
    webView = test->createWebView("is-controlled-by-automation", TRUE, nullptr);
    g_assert_true(webkit_web_view_is_controlled_by_automation(webView.get()));
    g_assert_cmpuint(webkit_web_view_get_automation_presentation_type(webView.get()), ==, WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_WINDOW);
#if ENABLE(2022_GLIB_API)
    g_assert_true(webkit_web_view_get_network_session(webView.get()) == networkSession);
#endif
    g_assert_true(test->createTopLevelBrowsingContext(webView.get()));

    auto newWebViewInWindow = test->createWebView(
        "is-controlled-by-automation", TRUE,
#if ENABLE(2022_GLIB_API)
        // Check also here that network session property is ignored when is-controlled-by-automation is true.
        "network-session", test->m_networkSession.get(),
#endif
        nullptr);
    g_assert_true(webkit_web_view_is_controlled_by_automation(newWebViewInWindow.get()));
    g_assert_cmpuint(webkit_web_view_get_automation_presentation_type(newWebViewInWindow.get()), ==, WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_WINDOW);
#if ENABLE(2022_GLIB_API)
    g_assert_true(webkit_web_view_get_network_session(newWebViewInWindow.get()) == networkSession);
#endif
    g_assert_true(test->createNewWindow(newWebViewInWindow.get()));

    auto newWebViewInTab = test->createWebView(
        "is-controlled-by-automation", TRUE,
        "automation-presentation-type", WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_TAB,
        nullptr);
    g_assert_true(webkit_web_view_is_controlled_by_automation(newWebViewInTab.get()));
    g_assert_cmpuint(webkit_web_view_get_automation_presentation_type(newWebViewInTab.get()), ==, WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_TAB);
    g_assert_true(test->createNewTab(newWebViewInTab.get()));

    checkNavigationCommandsAwaitTheirOwnNavigation(test);

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


static void serverCallback(SoupServer* server, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
{
    if (soup_server_message_get_method(message) != SOUP_METHOD_GET) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED, nullptr);
        return;
    }

    if (g_str_equal(path, "/stall")) {
        // Never answered, so a load of it stays in flight until another load replaces it.
        s_stalledLoadWasRequested = true;
        soup_server_pause_message(server, message);
        return;
    }

    if (g_str_equal(path, "/slow-replacement")) {
        // Held until the test answers it, so that the document it replaces stays active meanwhile.
        soup_server_pause_message(server, message);
        s_heldReplacementRequest = message;
        return;
    }

    if (g_str_equal(path, "/slow-image") || g_str_equal(path, "/slow-script")) {
        // Answered after a delay, which keeps the document embedding it loading meanwhile; a script also keeps
        // it parsing.
        soup_server_pause_message(server, message);
        g_timeout_add(500, [](gpointer userData) -> gboolean {
            auto* message = static_cast<SoupServerMessage*>(userData);
            if (g_str_has_suffix(g_uri_get_path(soup_server_message_get_uri(message)), "/slow-script"))
                s_slowScriptWasServed = true;
            soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
            soup_server_unpause_message(kServer->soupServer(), message);
            return G_SOURCE_REMOVE;
        }, message);
        return;
    }

    const char* html = "<p>target</p>";
    if (g_str_equal(path, "/fragment-while-loading"))
        html = "<img src='/slow-image'><script>history.pushState(null, '', '#changed');</script>";
    else if (g_str_equal(path, "/anchor-while-parsing"))
        html = "<script>location.hash = 'changed';</script><script src='/slow-script'></script><p>parsed</p>";
    else if (g_str_equal(path, "/slow-image-page"))
        html = "<img src='/slow-image'>";
    else if (g_str_equal(path, "/slow-script-page"))
        html = "<script src='/slow-script'></script><p>parsed</p>";
    soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    soup_message_headers_set_content_type(soup_server_message_get_response_headers(message), "text/html", nullptr);
    auto* responseBody = soup_server_message_get_response_body(message);
    soup_message_body_append(responseBody, SOUP_MEMORY_COPY, html, strlen(html));
    soup_message_body_complete(responseBody);
}

void beforeAll()
{
    g_setenv("WEBKIT_INSPECTOR_SERVER", "127.0.0.1:2229", TRUE);

    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

    GRefPtr<GSocketListener> listener = adoptGRef(g_socket_listener_new());
    s_unreachablePort = g_socket_listener_add_any_inet_port(listener.get(), nullptr, nullptr);
    g_assert_cmpuint(s_unreachablePort, !=, 0);
    g_socket_listener_close(listener.get());

    AutomationTest::add("WebKitAutomationSession", "request-session", testAutomationSessionRequestSession);
    Test::add("WebKitAutomationSession", "application-info", testAutomationSessionApplicationInfo);
}

void afterAll()
{
    delete kServer;
    g_unsetenv("WEBKIT_INSPECTOR_SERVER");
}
