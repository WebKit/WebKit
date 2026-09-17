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
#include <gio/gio.h>
#if ENABLE(WEBDRIVER_BIDI)
#include <wtf/JSONValues.h>
#endif
#include <wtf/UUID.h>
#if ENABLE(WEBDRIVER_BIDI)
#include <wtf/Vector.h>
#endif
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
            static_cast<AutomationTest*>(userData)->setConnection(SocketConnection::create(WTF::move(connection), s_messageHandlers, userData));
        }, this);
        g_main_loop_run(m_mainLoop.get());
    }

    ~AutomationTest()
    {
        if (m_connection)
            m_connection->close();
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
#if ENABLE(WEBDRIVER_BIDI)
        auto transportValue = JSON::Value::parseJSON(String::fromUTF8(message));
        auto transportMessage = transportValue ? transportValue->asObject() : nullptr;
        if (transportMessage && transportMessage->getString("method"_s) == "Automation.bidiMessageSent"_s) {
            if (auto parameters = transportMessage->getObject("params"_s)) {
                auto bidiValue = JSON::Value::parseJSON(parameters->getString("message"_s));
                if (auto bidiMessage = bidiValue ? bidiValue->asObject() : nullptr)
                    m_bidiMessages.append(bidiMessage.releaseNonNull());
            }
        }
#endif
        m_message = message;
        g_main_loop_quit(m_mainLoop.get());
    }

    void sendCommandToBackend(const String& command, const String& parameters = String())
    {
        static long sequenceID = 0;
        StringBuilder messageBuilder;
        messageBuilder.append("{\"id\":"_s, ++sequenceID, ",\"method\":\"Automation."_s, command, '"');
        if (!parameters.isNull())
            messageBuilder.append(",\"params\":"_s, parameters);
        messageBuilder.append('}');
        m_connection->sendMessage("SendMessageToBackend", g_variant_new("(tts)", m_connectionID, m_target.id, messageBuilder.toString().utf8().legacyCStringPointer()));
    }

#if ENABLE(WEBDRIVER_BIDI)
    String browsingContextHandleFromLastResponse() const
    {
        auto responseValue = JSON::Value::parseJSON(String::fromUTF8(m_message.data()));
        g_assert_true(!!responseValue);
        auto response = responseValue->asObject();
        g_assert_true(!!response);
        auto result = response->getObject("result"_s);
        g_assert_true(!!result);
        auto browsingContext = result->getString("handle"_s);
        g_assert_false(browsingContext.isEmpty());
        return browsingContext;
    }

    void loadHTMLAndWaitForTitle(WebKitWebView* webView, const char* html, const char* expectedTitle)
    {
        struct LoadState {
            GMainLoop* mainLoop;
            const char* expectedTitle;
            bool didFinish { false };
            bool timedOut { false };
        } loadState { m_mainLoop.get(), expectedTitle };
        auto loadChangedHandler = g_signal_connect(webView, "load-changed", G_CALLBACK(+[](WebKitWebView* webView, WebKitLoadEvent loadEvent, LoadState* loadState) {
            if (loadEvent != WEBKIT_LOAD_FINISHED)
                return;
            loadState->didFinish = true;
            if (!g_strcmp0(webkit_web_view_get_title(webView), loadState->expectedTitle))
                g_main_loop_quit(loadState->mainLoop);
        }), &loadState);
        auto titleChangedHandler = g_signal_connect(webView, "notify::title", G_CALLBACK(+[](WebKitWebView* webView, GParamSpec*, LoadState* loadState) {
            if (loadState->didFinish && !g_strcmp0(webkit_web_view_get_title(webView), loadState->expectedTitle))
                g_main_loop_quit(loadState->mainLoop);
        }), &loadState);
        auto timeoutID = g_timeout_add_seconds(10, [](gpointer userData) -> gboolean {
            auto& loadState = *static_cast<LoadState*>(userData);
            loadState.timedOut = true;
            g_main_loop_quit(loadState.mainLoop);
            return G_SOURCE_REMOVE;
        }, &loadState);

        webkit_web_view_load_html(webView, html, nullptr);
        while (!loadState.timedOut && (!loadState.didFinish || g_strcmp0(webkit_web_view_get_title(webView), expectedTitle)))
            g_main_loop_run(m_mainLoop.get());
        if (!loadState.timedOut)
            g_source_remove(timeoutID);
        g_signal_handler_disconnect(webView, loadChangedHandler);
        g_signal_handler_disconnect(webView, titleChangedHandler);
        if (loadState.timedOut)
            g_test_message("Timed out waiting for title '%s'; current title is '%s'", expectedTitle, webkit_web_view_get_title(webView));
        g_assert_false(loadState.timedOut);
        g_assert_cmpstr(webkit_web_view_get_title(webView), ==, expectedTitle);
    }

    Ref<JSON::Object> sendBidiCommandAndWait(int commandIdentifier, const String& method, Ref<JSON::Object>&& parameters)
    {
        auto command = JSON::Object::create();
        command->setInteger("id"_s, commandIdentifier);
        command->setString("method"_s, method);
        command->setObject("params"_s, WTF::move(parameters));

        auto automationParameters = JSON::Object::create();
        automationParameters->setString("message"_s, command->toJSONString());
        sendCommandToBackend("processBidiMessage"_s, automationParameters->toJSONString());

        auto response = waitForBidiMessage([commandIdentifier](const JSON::Object& message) {
            auto responseIdentifier = message.getInteger("id"_s);
            return responseIdentifier && *responseIdentifier == commandIdentifier;
        });
        g_assert_true(!!response);
        g_assert_true(response->getString("type"_s) == "success"_s);
        return response.releaseNonNull();
    }

    template<typename Predicate>
    RefPtr<JSON::Object> takeBidiMessage(Predicate&& predicate)
    {
        for (size_t index = 0; index < m_bidiMessages.size(); ++index) {
            if (!predicate(m_bidiMessages[index].get()))
                continue;
            auto message = m_bidiMessages[index].copyRef();
            m_bidiMessages.removeAt(index);
            return message;
        }
        return nullptr;
    }

    template<typename Predicate>
    RefPtr<JSON::Object> waitForBidiMessage(Predicate&& predicate)
    {
        struct WaitState {
            GMainLoop* mainLoop;
            bool timedOut { false };
        } waitState { m_mainLoop.get() };
        auto timeoutID = g_timeout_add_seconds(10, [](gpointer userData) -> gboolean {
            auto& waitState = *static_cast<WaitState*>(userData);
            waitState.timedOut = true;
            g_main_loop_quit(waitState.mainLoop);
            return G_SOURCE_REMOVE;
        }, &waitState);

        while (!waitState.timedOut) {
            if (auto message = takeBidiMessage(predicate)) {
                g_source_remove(timeoutID);
                return message;
            }
            g_main_loop_run(m_mainLoop.get());
        }

        for (auto& message : m_bidiMessages) {
            auto serializedMessage = message->toJSONString().utf8();
            g_test_message("Unmatched BiDi message: %s", reinterpret_cast<const char*>(serializedMessage.data()));
        }
        return nullptr;
    }
#endif

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
        sendCommandToBackend("createBrowsingContext"_s);
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
        sendCommandToBackend("createBrowsingContext"_s, "{\"presentationHint\":\"Window\"}"_s);
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
        sendCommandToBackend("createBrowsingContext"_s, "{\"presentationHint\":\"Tab\"}"_s);
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
#if ENABLE(WEBDRIVER_BIDI)
    Vector<Ref<JSON::Object>> m_bidiMessages;
#endif
};

const SocketConnection::MessageHandlers AutomationTest::s_messageHandlers = {
    { "DidClose", std::pair<CString, SocketConnection::MessageCallback> { { },
        [](SocketConnection&, GVariant*, gpointer userData) {
            auto& test = *static_cast<AutomationTest*>(userData);
            test.m_connection = nullptr;
        } }
    },
    { "DidStartAutomationSession", std::pair<CString, SocketConnection::MessageCallback> { "(ss)",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& test = *static_cast<AutomationTest*>(userData);
            test.didStartAutomationSession(parameters);
        } }
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
        } }
    },
    { "SendMessageToFrontend", std::pair<CString, SocketConnection::MessageCallback> { "(tts)",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& test = *static_cast<AutomationTest*>(userData);
            guint64 connectionID, targetID;
            const char* message;
            g_variant_get(parameters, "(tt&s)", &connectionID, &targetID, &message);
            test.receivedMessage(connectionID, targetID, message);
        } }
    }
};

#if ENABLE(WEBDRIVER_BIDI)
static Ref<JSON::Object> preloadScriptParameters(const String& functionDeclaration)
{
    auto parameters = JSON::Object::create();
    parameters->setString("functionDeclaration"_s, functionDeclaration);
    return parameters;
}

static String addPreloadScript(AutomationTest& test, int commandIdentifier, Ref<JSON::Object>&& parameters)
{
    auto response = test.sendBidiCommandAndWait(commandIdentifier, "script.addPreloadScript"_s, WTF::move(parameters));
    auto result = response->getObject("result"_s);
    g_assert_true(!!result);
    auto scriptIdentifier = result->getString("script"_s);
    g_assert_false(scriptIdentifier.isEmpty());
    return scriptIdentifier;
}

static void removePreloadScript(AutomationTest& test, int commandIdentifier, const String& scriptIdentifier)
{
    auto parameters = JSON::Object::create();
    parameters->setString("script"_s, scriptIdentifier);
    test.sendBidiCommandAndWait(commandIdentifier, "script.removePreloadScript"_s, WTF::move(parameters));
}

static void testAutomationSessionPreloadScriptsRunAtDocumentStart(AutomationTest* test, gconstpointer)
{
    test->setupIfNeeded();

    auto firstScriptParameters = preloadScriptParameters("() => { window.preloadTrace = window.preloadTrace || []; window.preloadTrace.push('first'); }"_s);
    auto defaultUserContexts = JSON::Array::create();
    defaultUserContexts->pushString("default"_s);
    firstScriptParameters->setArray("userContexts"_s, WTF::move(defaultUserContexts));
    auto firstScriptIdentifier = addPreloadScript(*test, 1, WTF::move(firstScriptParameters));

    auto firstWebView = test->createWebView("is-controlled-by-automation", TRUE, nullptr);
    g_assert_true(test->createTopLevelBrowsingContext(firstWebView.get()));
    auto firstBrowsingContext = test->browsingContextHandleFromLastResponse();

    auto subscribeParameters = JSON::Object::create();
    auto events = JSON::Array::create();
    events->pushString("log.entryAdded"_s);
    subscribeParameters->setArray("events"_s, WTF::move(events));
    auto subscribeResponse = test->sendBidiCommandAndWait(2, "session.subscribe"_s, WTF::move(subscribeParameters));
    auto subscribeResult = subscribeResponse->getObject("result"_s);
    g_assert_true(!!subscribeResult);
    auto logSubscriptionIdentifier = subscribeResult->getString("subscription"_s);
    g_assert_false(logSubscriptionIdentifier.isEmpty());

    auto throwingScriptIdentifier = addPreloadScript(*test, 3, preloadScriptParameters("() => { throw new Error('expected preload failure'); }"_s));
    auto thirdScriptIdentifier = addPreloadScript(*test, 4, preloadScriptParameters("() => { window.preloadTrace = window.preloadTrace || []; window.preloadTrace.push('third'); }"_s));

    auto contextScriptParameters = preloadScriptParameters("() => { window.contextScopedPreload = true; }"_s);
    auto contexts = JSON::Array::create();
    contexts->pushString(firstBrowsingContext);
    contextScriptParameters->setArray("contexts"_s, WTF::move(contexts));
    auto contextScriptIdentifier = addPreloadScript(*test, 5, WTF::move(contextScriptParameters));

    auto removedScriptIdentifier = addPreloadScript(*test, 6, preloadScriptParameters("() => { window.removedPreloadRan = true; }"_s));
    removePreloadScript(*test, 7, removedScriptIdentifier);

    static constexpr auto initialDocument = "<!doctype html><script>document.title = (window.preloadTrace ? window.preloadTrace.join(',') : 'missing') + ':' + (window.contextScopedPreload === true) + ':' + (window.removedPreloadRan === true) + ':initial';</script>";
    auto waitForExpectedPreloadError = [test] {
        auto exceptionEvent = test->waitForBidiMessage([](const JSON::Object& message) {
            if (message.getString("method"_s) != "log.entryAdded"_s)
                return false;
            auto parameters = message.getObject("params"_s);
            return parameters && parameters->getString("text"_s) == "Error: expected preload failure"_s;
        });
        g_assert_true(!!exceptionEvent);
        g_assert_true(exceptionEvent->getObject("params"_s)->getString("level"_s) == "error"_s);
    };

    test->loadHTMLAndWaitForTitle(firstWebView.get(), initialDocument, "first,third:true:false:initial");
    waitForExpectedPreloadError();

    removePreloadScript(*test, 8, throwingScriptIdentifier);
    auto unsubscribeParameters = JSON::Object::create();
    auto subscriptions = JSON::Array::create();
    subscriptions->pushString(logSubscriptionIdentifier);
    unsubscribeParameters->setArray("subscriptions"_s, WTF::move(subscriptions));
    test->sendBidiCommandAndWait(9, "session.unsubscribe"_s, WTF::move(unsubscribeParameters));

    auto secondWebView = test->createWebView("is-controlled-by-automation", TRUE, nullptr);
    g_assert_true(test->createNewWindow(secondWebView.get()));

    test->loadHTMLAndWaitForTitle(secondWebView.get(), initialDocument, "first,third:false:false:initial");

    auto uncontrolledWebView = test->createWebView();
    test->loadHTMLAndWaitForTitle(uncontrolledWebView.get(), initialDocument, "missing:false:false:initial");

    removePreloadScript(*test, 10, firstScriptIdentifier);
    removePreloadScript(*test, 11, thirdScriptIdentifier);
    removePreloadScript(*test, 12, contextScriptIdentifier);
}
#endif

static void testAutomationSessionRequestSession(AutomationTest* test, gconstpointer)
{
    auto sessionID = createVersion4UUIDString().utf8();
    // WebKitAutomationSession::automation-started is never emitted if automation is not enabled.
    g_assert_false(webkit_web_context_is_automation_allowed(test->m_webContext.get()));
#if ENABLE(2022_GLIB_API)
    // Network session for automation is nullptr if automation is not enabled.
    g_assert_null(webkit_web_context_get_network_session_for_automation(test->m_webContext.get()));
#endif
    auto* session = test->requestSession(sessionID.legacyCStringPointer());
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

    session = test->requestSession(sessionID.legacyCStringPointer());
    g_assert_cmpstr(webkit_automation_session_get_id(session), ==, sessionID.legacyCStringPointer());
    g_assert_cmpuint(test->m_target.id, >, 0);
    ASSERT_CMP_CSTRING(test->m_target.name, ==, sessionID);
    g_assert_false(test->m_target.isPaired);

#if ENABLE(WEBDRIVER_BIDI)
    testAutomationSessionPreloadScriptsRunAtDocumentStart(test, nullptr);
#endif

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
