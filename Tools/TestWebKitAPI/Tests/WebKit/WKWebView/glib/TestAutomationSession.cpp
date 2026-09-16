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
#if ENABLE(WEBDRIVER_BIDI)
#include "WebKitTestServer.h"
#include "WebKitWebViewInternal.h"
#endif
#include <gio/gio.h>
#if ENABLE(WEBDRIVER_BIDI)
#include <wtf/JSONValues.h>
#endif
#include <wtf/UUID.h>
#include <wtf/Vector.h>
#include <wtf/glib/SocketConnection.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(WEBDRIVER_BIDI)
static std::unique_ptr<WebKitTestServer> s_workerRealmServer;

static void workerRealmServerCallback(SoupServer*, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
{
    static constexpr auto dedicatedWorkerDocument = "<title>loading</title>"
        "<script>"
        "window.worker = new Worker(\"/dedicated-worker.js\");"
        "worker.onmessage = () => document.title = \"ready\";"
        "</script>";
    static constexpr auto sharedWorkerDocument = "<title>loading</title>"
        "<script>"
        "window.worker = new SharedWorker('/shared-worker.js', 'automation-shared-worker');"
        "worker.port.onmessage = () => document.title = 'ready';"
        "worker.port.start();"
        "</script>";

    static constexpr auto sharedWorkerHostTerminationDocument = "<title>loading</title>"
        "<script>"
        "window.worker = new SharedWorker(\"/shared-worker-host-termination.js\", \"automation-shared-worker-host-termination\");"
        "worker.port.onmessage = () => document.title = \"ready\";"
        "worker.port.start();"
        "</script>";

    static constexpr auto serviceWorkerPage = R"HTML(
        <title>loading</title>
        <script>
            const registerServiceWorkerAndReportExecution = async() => {
                const queryParameters = new URLSearchParams(location.search);
                const requestedWorkerScript = queryParameters.get('workerScript');
                const clientRequestIdentifier = queryParameters.get('clientRequest');
                if (requestedWorkerScript) {
                    const requestedWorkerScriptPath = '/' + requestedWorkerScript;
                    const registration = await navigator.serviceWorker.register(requestedWorkerScriptPath);
                    while (!registration.active || !registration.active.scriptURL.endsWith(requestedWorkerScriptPath))
                        await new Promise(resolve => navigator.serviceWorker.addEventListener('controllerchange', resolve, { once: true }));
                }
                const registration = await navigator.serviceWorker.ready;
                const channel = new MessageChannel();
                channel.port1.onmessage = event => document.title = clientRequestIdentifier + ':' + event.data;
                registration.active.postMessage(null, [channel.port2]);
            };
            registerServiceWorkerAndReportExecution();
        </script>
    )HTML";
    static constexpr auto serviceWorkerScriptV1 = R"JS(
        const realmExecutionToken = 'service-worker-execution-v1-' + Date.now() + '-' + Math.random();
        self.addEventListener('install', event => event.waitUntil(self.skipWaiting()));
        self.addEventListener('activate', event => event.waitUntil(self.clients.claim()));
        self.addEventListener('message', event => event.ports[0].postMessage(realmExecutionToken));
    )JS";
    static constexpr auto serviceWorkerScriptV2 = R"JS(
        const realmExecutionToken = 'service-worker-execution-v2-' + Date.now() + '-' + Math.random();
        self.addEventListener('install', event => event.waitUntil(self.skipWaiting()));
        self.addEventListener('activate', event => event.waitUntil(self.clients.claim()));
        self.addEventListener('message', event => event.ports[0].postMessage(realmExecutionToken));
    )JS";

    const char* content = nullptr;
    const char* contentType = "text/html";
    if (g_str_equal(path, "/dedicated-worker.html"))
        content = dedicatedWorkerDocument;
    else if (g_str_equal(path, "/dedicated-worker.js")) {
        content = "postMessage('ready');";
        contentType = "application/javascript";
    } else if (g_str_equal(path, "/service-worker.html"))
        content = serviceWorkerPage;
    else if (g_str_equal(path, "/service-worker-v1.js")) {
        content = serviceWorkerScriptV1;
        contentType = "application/javascript";
    } else if (g_str_equal(path, "/service-worker-v2.js")) {
        content = serviceWorkerScriptV2;
        contentType = "application/javascript";
    } else if (g_str_equal(path, "/shared-worker.html"))
        content = sharedWorkerDocument;
    else if (g_str_equal(path, "/shared-worker.js")) {
        content = "onconnect = event => event.ports[0].postMessage('ready');";
        contentType = "application/javascript";
    } else if (g_str_equal(path, "/shared-worker-host-termination.html"))
        content = sharedWorkerHostTerminationDocument;
    else if (g_str_equal(path, "/shared-worker-host-termination.js")) {
        content = "onconnect = event => event.ports[0].postMessage(\"ready\");";
        contentType = "application/javascript";
    } else if (g_str_equal(path, "/empty.html"))
        content = "<title>empty</title>";
    else {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
        return;
    }

    soup_server_message_set_response(message, contentType, SOUP_MEMORY_STATIC, content, strlen(content));
    soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
}
#endif

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
    String loadServiceWorkerClientAndGetExecutionToken(WebKitWebView* webView, const char* path, const char* clientRequestIdentifier)
    {
        GUniquePtr<char> expectedTitlePrefix(g_strdup_printf("%s:", clientRequestIdentifier));
        struct LoadState {
            GMainLoop* mainLoop;
            const char* expectedTitlePrefix;
            String executionToken { };
            bool timedOut { false };
        } loadState { m_mainLoop.get(), expectedTitlePrefix.get() };

        auto titleChangedHandler = g_signal_connect(webView, "notify::title", G_CALLBACK(+[](WebKitWebView* webView, GParamSpec*, LoadState* loadState) {
            const char* title = webkit_web_view_get_title(webView);
            if (!title || !g_str_has_prefix(title, loadState->expectedTitlePrefix))
                return;
            loadState->executionToken = String::fromUTF8(title + strlen(loadState->expectedTitlePrefix));
            g_main_loop_quit(loadState->mainLoop);
        }), &loadState);
        auto timeoutID = g_timeout_add_seconds(10, [](gpointer userData) -> gboolean {
            auto& loadState = *static_cast<LoadState*>(userData);
            loadState.timedOut = true;
            g_main_loop_quit(loadState.mainLoop);
            return G_SOURCE_REMOVE;
        }, &loadState);

        webkit_web_view_load_uri(webView, s_workerRealmServer->getURIForPath(path).legacyCStringPointer());
        while (loadState.executionToken.isEmpty() && !loadState.timedOut)
            g_main_loop_run(m_mainLoop.get());
        if (!loadState.timedOut)
            g_source_remove(timeoutID);
        g_signal_handler_disconnect(webView, titleChangedHandler);
        g_assert_false(loadState.executionToken.isEmpty());
        return loadState.executionToken;
    }

    Ref<JSON::Object> sendBidiCommandAndWait(int identifier, const String& method, Ref<JSON::Object>&& parameters)
    {
        auto command = JSON::Object::create();
        command->setInteger("id"_s, identifier);
        command->setString("method"_s, method);
        command->setObject("params"_s, WTF::move(parameters));

        auto automationParameters = JSON::Object::create();
        automationParameters->setString("message"_s, command->toJSONString());
        sendCommandToBackend("processBidiMessage"_s, automationParameters->toJSONString());

        auto response = waitForBidiMessage([identifier](const JSON::Object& message) {
            auto messageIdentifier = message.getInteger("id"_s);
            return messageIdentifier && *messageIdentifier == identifier;
        });
        g_assert_true(response->getString("type"_s) == "success"_s);
        return response;
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
    Ref<JSON::Object> waitForBidiMessage(Predicate&& predicate)
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
                return message.releaseNonNull();
            }
            g_main_loop_run(m_mainLoop.get());
        }
        g_assert_not_reached();
    }
#endif

#if ENABLE(WEBDRIVER_BIDI)
    String browsingContextHandleFromLastResponse() const
    {
        auto responseValue = JSON::Value::parseJSON(String::fromUTF8(m_message.data()));
        auto response = responseValue ? responseValue->asObject() : nullptr;
        auto result = response ? response->getObject("result"_s) : nullptr;
        auto browsingContext = result ? result->getString("handle"_s) : nullString();
        g_assert_false(browsingContext.isEmpty());
        return browsingContext;
    }

    void loadPageAndWaitForTitle(WebKitWebView* webView, const CString& uri, const char* expectedTitle)
    {
        struct LoadState {
            GMainLoop* mainLoop;
            const char* expectedTitle;
            bool ready { false };
            bool timedOut { false };
        } loadState { m_mainLoop.get(), expectedTitle };

        auto titleChangedHandler = g_signal_connect(webView, "notify::title", G_CALLBACK(+[](WebKitWebView* webView, GParamSpec*, LoadState* loadState) {
            if (!g_strcmp0(webkit_web_view_get_title(webView), loadState->expectedTitle)) {
                loadState->ready = true;
                g_main_loop_quit(loadState->mainLoop);
            }
        }), &loadState);
        auto timeoutID = g_timeout_add_seconds(10, [](gpointer userData) -> gboolean {
            auto& loadState = *static_cast<LoadState*>(userData);
            loadState.timedOut = true;
            g_main_loop_quit(loadState.mainLoop);
            return G_SOURCE_REMOVE;
        }, &loadState);

        webkit_web_view_load_uri(webView, uri.legacyCStringPointer());
        while (!loadState.ready && !loadState.timedOut)
            g_main_loop_run(m_mainLoop.get());
        if (!loadState.timedOut)
            g_source_remove(timeoutID);
        g_signal_handler_disconnect(webView, titleChangedHandler);
        g_assert_true(loadState.ready);
    }

    Ref<JSON::Array> getRealms(int commandIdentifier, Ref<JSON::Object>&& parameters)
    {
        auto response = sendBidiCommandAndWait(commandIdentifier, "script.getRealms"_s, WTF::move(parameters));
        auto result = response->getObject("result"_s);
        auto realms = result ? result->getArray("realms"_s) : nullptr;
        g_assert_true(!!realms);
        return realms.releaseNonNull();
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

        return String::fromUTF8(m_message.span()).contains("\"presentation\":\"Tab\""_s);
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
        }}
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
static void verifyServiceWorkerRealmLifecycleAndEnumeration(AutomationTest&, WebKitWebView*, const String&, WebKitWebView*);
static void verifySharedWorkerRealmLifecycle(AutomationTest&, GRefPtr<WebKitWebView>&, const String&, GRefPtr<WebKitWebView>&, const String&);
static void verifyDetachedSharedWorkerOwnerDoesNotReceiveRealmDestroyed(AutomationTest&, GRefPtr<WebKitWebView>&, const String&, GRefPtr<WebKitWebView>&, const String&);
static void verifySharedWorkerHostProcessTermination(AutomationTest&, GRefPtr<WebKitWebView>&, const String&);
static void waitUntilNoSharedWorkerRealms(AutomationTest&, int initialCommandIdentifier);
#endif


#if ENABLE(WEBDRIVER_BIDI)
static void verifyDedicatedWorkerRealmEnumeration(AutomationTest& test, WebKitWebView* webView, const String& workerBrowsingContext, const String& browsingContextWithoutWorker)
{
    test.loadPageAndWaitForTitle(webView, s_workerRealmServer->getURIForPath("/dedicated-worker.html"), "ready");

    auto windowParameters = JSON::Object::create();
    windowParameters->setString("context"_s, workerBrowsingContext);
    windowParameters->setString("type"_s, "window"_s);
    auto windowRealms = test.getRealms(1, WTF::move(windowParameters));
    g_assert_cmpuint(windowRealms->length(), ==, 1);
    auto windowRealm = windowRealms->get(0)->asObject();
    g_assert_true(!!windowRealm);
    auto windowRealmIdentifier = windowRealm->getString("realm"_s);
    g_assert_false(windowRealmIdentifier.isEmpty());

    auto contextWithoutWorkerParameters = JSON::Object::create();
    contextWithoutWorkerParameters->setString("context"_s, browsingContextWithoutWorker);
    contextWithoutWorkerParameters->setString("type"_s, "dedicated-worker"_s);
    g_assert_cmpuint(test.getRealms(2, WTF::move(contextWithoutWorkerParameters))->length(), ==, 0);

    auto workerParameters = JSON::Object::create();
    workerParameters->setString("context"_s, workerBrowsingContext);
    workerParameters->setString("type"_s, "dedicated-worker"_s);
    auto workerRealms = test.getRealms(3, WTF::move(workerParameters));
    g_assert_cmpuint(workerRealms->length(), ==, 1);
    auto workerRealm = workerRealms->get(0)->asObject();
    g_assert_true(!!workerRealm);
    g_assert_true(workerRealm->getString("type"_s) == "dedicated-worker"_s);
    g_assert_true(workerRealm->getString("origin"_s) == s_workerRealmServer->baseURL().protocolHostAndPort());
    g_assert_false(!!workerRealm->getValue("context"_s));
    auto workerRealmIdentifier = workerRealm->getString("realm"_s);
    g_assert_false(workerRealmIdentifier.isEmpty());
    auto owners = workerRealm->getArray("owners"_s);
    g_assert_true(!!owners);
    g_assert_cmpuint(owners->length(), ==, 1);
    g_assert_true(owners->get(0)->asString() == windowRealmIdentifier);

    auto allRealms = test.getRealms(4, JSON::Object::create());
    unsigned matchingWorkerRealmCount = 0;
    for (size_t index = 0; index < allRealms->length(); ++index) {
        auto realm = allRealms->get(index)->asObject();
        if (realm && realm->getString("type"_s) == "dedicated-worker"_s) {
            ++matchingWorkerRealmCount;
            g_assert_true(realm->getString("realm"_s) == workerRealmIdentifier);
        }
    }
    g_assert_cmpuint(matchingWorkerRealmCount, ==, 1);

    test.loadPageAndWaitForTitle(webView, s_workerRealmServer->getURIForPath("/empty.html"), "empty");
    auto terminatedWorkerParameters = JSON::Object::create();
    terminatedWorkerParameters->setString("context"_s, workerBrowsingContext);
    terminatedWorkerParameters->setString("type"_s, "dedicated-worker"_s);
    g_assert_cmpuint(test.getRealms(5, WTF::move(terminatedWorkerParameters))->length(), ==, 0);
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
#if ENABLE(WEBDRIVER_BIDI)
    auto firstBrowsingContext = test->browsingContextHandleFromLastResponse();
#endif

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
#if ENABLE(WEBDRIVER_BIDI)
    auto secondBrowsingContext = test->browsingContextHandleFromLastResponse();
    verifyDedicatedWorkerRealmEnumeration(*test, webView.get(), firstBrowsingContext, secondBrowsingContext);
#endif

    auto newWebViewInTab = test->createWebView(
        "is-controlled-by-automation", TRUE,
        "automation-presentation-type", WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_TAB,
        nullptr);
    g_assert_true(webkit_web_view_is_controlled_by_automation(newWebViewInTab.get()));
    g_assert_cmpuint(webkit_web_view_get_automation_presentation_type(newWebViewInTab.get()), ==, WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_TAB);
    g_assert_true(test->createNewTab(newWebViewInTab.get()));
#if ENABLE(WEBDRIVER_BIDI)
    auto thirdBrowsingContext = test->browsingContextHandleFromLastResponse();
    verifyServiceWorkerRealmLifecycleAndEnumeration(*test, webView.get(), firstBrowsingContext, newWebViewInWindow.get());
    verifySharedWorkerRealmLifecycle(*test, webView, firstBrowsingContext, newWebViewInWindow, secondBrowsingContext);

    auto secondWebViewInTab = test->createWebView(
        "is-controlled-by-automation", TRUE,
        "automation-presentation-type", WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_TAB,
        nullptr);
    g_assert_true(test->createNewTab(secondWebViewInTab.get()));
    auto fourthBrowsingContext = test->browsingContextHandleFromLastResponse();
    verifyDetachedSharedWorkerOwnerDoesNotReceiveRealmDestroyed(*test, newWebViewInTab, thirdBrowsingContext, secondWebViewInTab, fourthBrowsingContext);

    auto sharedWorkerHostTerminationWebView = test->createWebView(
        "is-controlled-by-automation", TRUE,
        "automation-presentation-type", WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_TAB,
        nullptr);
    g_assert_true(test->createNewTab(sharedWorkerHostTerminationWebView.get()));
    auto fifthBrowsingContext = test->browsingContextHandleFromLastResponse();
    verifySharedWorkerHostProcessTermination(*test, sharedWorkerHostTerminationWebView, fifthBrowsingContext);
#endif

    webkit_web_context_set_automation_allowed(test->m_webContext.get(), FALSE);
}

#if ENABLE(WEBDRIVER_BIDI)
static String assertServiceWorkerRealmAndGetIdentifier(const JSON::Object& realm, const String& expectedOrigin)
{
    g_assert_true(realm.getString("type"_s) == "service-worker"_s);
    g_assert_true(realm.getString("origin"_s) == expectedOrigin);
    g_assert_false(!!realm.getValue("context"_s));
    g_assert_false(!!realm.getValue("owners"_s));

    auto realmIdentifier = realm.getString("realm"_s);
    g_assert_true(realmIdentifier.startsWith("realm-"_s));
    return realmIdentifier;
}

static Ref<JSON::Object> waitForServiceWorkerRealmCreated(AutomationTest& test)
{
    auto event = test.waitForBidiMessage([](const JSON::Object& message) {
        if (message.getString("method"_s) != "script.realmCreated"_s)
            return false;
        auto parameters = message.getObject("params"_s);
        return parameters && parameters->getString("type"_s) == "service-worker"_s;
    });
    auto realm = event->getObject("params"_s);
    g_assert_true(!!realm);
    return realm.releaseNonNull();
}

static void waitForServiceWorkerRealmDestroyed(AutomationTest& test, const String& realmIdentifier)
{
    auto event = test.waitForBidiMessage([&realmIdentifier](const JSON::Object& message) {
        if (message.getString("method"_s) != "script.realmDestroyed"_s)
            return false;
        auto parameters = message.getObject("params"_s);
        return parameters && parameters->getString("realm"_s) == realmIdentifier;
    });
    auto realm = event->getObject("params"_s);
    g_assert_true(!!realm);
    g_assert_false(!!realm->getValue("context"_s));
}

static void assertContainsSingleServiceWorkerRealm(const JSON::Array& realms, const String& expectedRealmIdentifier, const String& expectedOrigin)
{
    unsigned serviceWorkerRealmCount = 0;
    for (size_t index = 0; index < realms.length(); ++index) {
        auto realm = realms.get(index)->asObject();
        if (!realm || realm->getString("type"_s) != "service-worker"_s)
            continue;
        ++serviceWorkerRealmCount;
        g_assert_true(assertServiceWorkerRealmAndGetIdentifier(*realm, expectedOrigin) == expectedRealmIdentifier);
    }
    g_assert_cmpuint(serviceWorkerRealmCount, ==, 1);
}

static void assertContainsNoServiceWorkerRealm(const JSON::Array& realms)
{
    for (size_t index = 0; index < realms.length(); ++index) {
        auto realm = realms.get(index)->asObject();
        g_assert_true(!realm || realm->getString("type"_s) != "service-worker"_s);
    }
}

static void verifyServiceWorkerRealmEnumeration(AutomationTest& test, int initialCommandIdentifier, const String& browsingContext, const String& expectedRealmIdentifier, const String& expectedOrigin)
{
    auto typeFilterParameters = JSON::Object::create();
    typeFilterParameters->setString("type"_s, "service-worker"_s);
    auto serviceWorkerRealms = test.getRealms(initialCommandIdentifier, WTF::move(typeFilterParameters));
    g_assert_cmpuint(serviceWorkerRealms->length(), ==, 1);
    assertContainsSingleServiceWorkerRealm(serviceWorkerRealms, expectedRealmIdentifier, expectedOrigin);

    auto allRealms = test.getRealms(initialCommandIdentifier + 1, JSON::Object::create());
    assertContainsSingleServiceWorkerRealm(allRealms, expectedRealmIdentifier, expectedOrigin);

    auto contextFilterParameters = JSON::Object::create();
    contextFilterParameters->setString("context"_s, browsingContext);
    contextFilterParameters->setString("type"_s, "service-worker"_s);
    g_assert_cmpuint(test.getRealms(initialCommandIdentifier + 2, WTF::move(contextFilterParameters))->length(), ==, 0);

    auto contextOnlyFilterParameters = JSON::Object::create();
    contextOnlyFilterParameters->setString("context"_s, browsingContext);
    assertContainsNoServiceWorkerRealm(test.getRealms(initialCommandIdentifier + 3, WTF::move(contextOnlyFilterParameters)));
}

static void verifyServiceWorkerRealmLifecycleAndEnumeration(AutomationTest& test, WebKitWebView* firstWebView, const String& firstBrowsingContext, WebKitWebView* secondWebView)
{
    auto subscriptionParameters = JSON::Object::create();
    auto events = JSON::Array::create();
    events->pushString("script.realmCreated"_s);
    events->pushString("script.realmDestroyed"_s);
    subscriptionParameters->setArray("events"_s, WTF::move(events));
    test.sendBidiCommandAndWait(2000, "session.subscribe"_s, WTF::move(subscriptionParameters));

    auto firstExecutionToken = test.loadServiceWorkerClientAndGetExecutionToken(firstWebView,
        "/service-worker.html?workerScript=service-worker-v1.js&clientRequest=initial", "initial");
    auto firstRealm = waitForServiceWorkerRealmCreated(test);
    auto expectedOrigin = s_workerRealmServer->baseURL().protocolHostAndPort();
    auto firstRealmIdentifier = assertServiceWorkerRealmAndGetIdentifier(firstRealm, expectedOrigin);

    auto secondClientExecutionToken = test.loadServiceWorkerClientAndGetExecutionToken(secondWebView,
        "/service-worker.html?clientRequest=second-client", "second-client");
    g_assert_true(secondClientExecutionToken == firstExecutionToken);
    verifyServiceWorkerRealmEnumeration(test, 2001, firstBrowsingContext, firstRealmIdentifier, expectedOrigin);

    auto executionTokenAfterNavigation = test.loadServiceWorkerClientAndGetExecutionToken(firstWebView,
        "/service-worker.html?clientRequest=after-navigation", "after-navigation");
    g_assert_true(executionTokenAfterNavigation == firstExecutionToken);
    g_assert_false(!!test.takeBidiMessage([&firstRealmIdentifier](const JSON::Object& message) {
        if (message.getString("method"_s) != "script.realmDestroyed"_s)
            return false;
        auto parameters = message.getObject("params"_s);
        return parameters && parameters->getString("realm"_s) == firstRealmIdentifier;
    }));

    auto replacementExecutionToken = test.loadServiceWorkerClientAndGetExecutionToken(firstWebView,
        "/service-worker.html?workerScript=service-worker-v2.js&clientRequest=updated", "updated");
    auto replacementRealm = waitForServiceWorkerRealmCreated(test);
    auto replacementRealmIdentifier = assertServiceWorkerRealmAndGetIdentifier(replacementRealm, expectedOrigin);
    waitForServiceWorkerRealmDestroyed(test, firstRealmIdentifier);
    g_assert_true(replacementExecutionToken != firstExecutionToken);
    g_assert_true(replacementRealmIdentifier != firstRealmIdentifier);
    verifyServiceWorkerRealmEnumeration(test, 2005, firstBrowsingContext, replacementRealmIdentifier, expectedOrigin);

    auto unsubscribeParameters = JSON::Object::create();
    auto unsubscribedEvents = JSON::Array::create();
    unsubscribedEvents->pushString("script.realmCreated"_s);
    unsubscribedEvents->pushString("script.realmDestroyed"_s);
    unsubscribeParameters->setArray("events"_s, WTF::move(unsubscribedEvents));
    test.sendBidiCommandAndWait(2009, "session.unsubscribe"_s, WTF::move(unsubscribeParameters));
}

static String assertSharedWorkerRealmAndGetIdentifier(const JSON::Object& realm, const String& expectedOrigin)
{
    g_assert_true(realm.getString("type"_s) == "shared-worker"_s);
    g_assert_true(realm.getString("origin"_s) == expectedOrigin);
    g_assert_false(!!realm.getValue("context"_s));
    g_assert_false(!!realm.getValue("owners"_s));

    auto realmIdentifier = realm.getString("realm"_s);
    g_assert_true(realmIdentifier.startsWith("realm-"_s));
    return realmIdentifier;
}

static String getSharedWorkerRealmIdentifier(AutomationTest& test, int commandIdentifier, const String& browsingContext, const String& expectedOrigin)
{
    auto parameters = JSON::Object::create();
    parameters->setString("context"_s, browsingContext);
    parameters->setString("type"_s, "shared-worker"_s);
    auto realms = test.getRealms(commandIdentifier, WTF::move(parameters));
    g_assert_cmpuint(realms->length(), ==, 1);

    auto realm = realms->get(0)->asObject();
    g_assert_true(!!realm);
    return assertSharedWorkerRealmAndGetIdentifier(*realm, expectedOrigin);
}

static void waitUntilNoSharedWorkerRealms(AutomationTest& test, int initialCommandIdentifier)
{
    bool didObserveNoSharedWorkerRealms = false;
    auto deadline = g_get_monotonic_time() + 10 * G_USEC_PER_SEC;
    while (g_get_monotonic_time() < deadline) {
        auto parameters = JSON::Object::create();
        parameters->setString("type"_s, "shared-worker"_s);
        if (!test.getRealms(initialCommandIdentifier++, WTF::move(parameters))->length()) {
            didObserveNoSharedWorkerRealms = true;
            break;
        }
    }
    g_assert_true(didObserveNoSharedWorkerRealms);
}

static void verifySharedWorkerRealmLifecycle(AutomationTest& test, GRefPtr<WebKitWebView>& firstWebView, const String& firstBrowsingContext, GRefPtr<WebKitWebView>& secondWebView, const String& secondBrowsingContext)
{
    g_assert_true(firstBrowsingContext != secondBrowsingContext);
    test.loadPageAndWaitForTitle(firstWebView.get(), s_workerRealmServer->getURIForPath("/shared-worker.html"), "ready");
    test.loadPageAndWaitForTitle(secondWebView.get(), s_workerRealmServer->getURIForPath("/shared-worker.html"), "ready");

    auto subscriptionParameters = JSON::Object::create();
    auto events = JSON::Array::create();
    events->pushString("script.realmCreated"_s);
    subscriptionParameters->setArray("events"_s, WTF::move(events));
    test.sendBidiCommandAndWait(6, "session.subscribe"_s, WTF::move(subscriptionParameters));

    auto realmCreatedEvent = test.waitForBidiMessage([](const JSON::Object& message) {
        if (message.getString("method"_s) != "script.realmCreated"_s)
            return false;
        auto parameters = message.getObject("params"_s);
        return parameters && parameters->getString("type"_s) == "shared-worker"_s;
    });
    auto expectedOrigin = s_workerRealmServer->baseURL().protocolHostAndPort();
    auto createdRealm = realmCreatedEvent->getObject("params"_s);
    g_assert_true(!!createdRealm);
    auto realmIdentifier = assertSharedWorkerRealmAndGetIdentifier(*createdRealm, expectedOrigin);

    g_assert_true(getSharedWorkerRealmIdentifier(test, 7, firstBrowsingContext, expectedOrigin) == realmIdentifier);
    g_assert_true(getSharedWorkerRealmIdentifier(test, 8, secondBrowsingContext, expectedOrigin) == realmIdentifier);

    auto allRealms = test.getRealms(9, JSON::Object::create());
    unsigned sharedWorkerRealmCount = 0;
    for (size_t index = 0; index < allRealms->length(); ++index) {
        auto realm = allRealms->get(index)->asObject();
        if (!realm || realm->getString("type"_s) != "shared-worker"_s)
            continue;
        ++sharedWorkerRealmCount;
        g_assert_true(assertSharedWorkerRealmAndGetIdentifier(*realm, expectedOrigin) == realmIdentifier);
    }
    g_assert_cmpuint(sharedWorkerRealmCount, ==, 1);

    auto destructionSubscriptionParameters = JSON::Object::create();
    auto destructionEvents = JSON::Array::create();
    destructionEvents->pushString("script.realmDestroyed"_s);
    destructionSubscriptionParameters->setArray("events"_s, WTF::move(destructionEvents));
    auto destructionContexts = JSON::Array::create();
    destructionContexts->pushString(secondBrowsingContext);
    destructionSubscriptionParameters->setArray("contexts"_s, WTF::move(destructionContexts));
    test.sendBidiCommandAndWait(10, "session.subscribe"_s, WTF::move(destructionSubscriptionParameters));

    firstWebView = nullptr;
    g_assert_true(getSharedWorkerRealmIdentifier(test, 11, secondBrowsingContext, expectedOrigin) == realmIdentifier);

    secondWebView = nullptr;

    auto realmDestroyedEvent = test.waitForBidiMessage([&realmIdentifier](const JSON::Object& message) {
        if (message.getString("method"_s) != "script.realmDestroyed"_s)
            return false;
        auto parameters = message.getObject("params"_s);
        return parameters && parameters->getString("realm"_s) == realmIdentifier;
    });
    auto destroyedRealm = realmDestroyedEvent->getObject("params"_s);
    g_assert_true(!!destroyedRealm);
    g_assert_false(!!destroyedRealm->getValue("context"_s));

    auto sharedWorkerParameters = JSON::Object::create();
    sharedWorkerParameters->setString("type"_s, "shared-worker"_s);
    g_assert_cmpuint(test.getRealms(12, WTF::move(sharedWorkerParameters))->length(), ==, 0);
}

static void verifyDetachedSharedWorkerOwnerDoesNotReceiveRealmDestroyed(AutomationTest& test, GRefPtr<WebKitWebView>& firstWebView, const String& firstBrowsingContext, GRefPtr<WebKitWebView>& secondWebView, const String& secondBrowsingContext)
{
    test.loadPageAndWaitForTitle(firstWebView.get(), s_workerRealmServer->getURIForPath("/shared-worker.html"), "ready");
    test.loadPageAndWaitForTitle(secondWebView.get(), s_workerRealmServer->getURIForPath("/shared-worker.html"), "ready");

    auto expectedOrigin = s_workerRealmServer->baseURL().protocolHostAndPort();
    auto realmIdentifier = getSharedWorkerRealmIdentifier(test, 13, firstBrowsingContext, expectedOrigin);
    g_assert_true(getSharedWorkerRealmIdentifier(test, 14, secondBrowsingContext, expectedOrigin) == realmIdentifier);

    auto subscriptionParameters = JSON::Object::create();
    auto events = JSON::Array::create();
    events->pushString("script.realmDestroyed"_s);
    subscriptionParameters->setArray("events"_s, WTF::move(events));
    auto contexts = JSON::Array::create();
    contexts->pushString(firstBrowsingContext);
    subscriptionParameters->setArray("contexts"_s, WTF::move(contexts));
    test.sendBidiCommandAndWait(15, "session.subscribe"_s, WTF::move(subscriptionParameters));

    firstWebView = nullptr;
    g_assert_true(getSharedWorkerRealmIdentifier(test, 16, secondBrowsingContext, expectedOrigin) == realmIdentifier);
    secondWebView = nullptr;

    // The empty getRealms response is ordered after lifecycle messages from the
    // worker-hosting process, so it also barriers realmDestroyed delivery.
    waitUntilNoSharedWorkerRealms(test, 17);

    auto unexpectedRealmDestroyedEvent = test.takeBidiMessage([&realmIdentifier](const JSON::Object& message) {
        if (message.getString("method"_s) != "script.realmDestroyed"_s)
            return false;
        auto parameters = message.getObject("params"_s);
        return parameters && parameters->getString("realm"_s) == realmIdentifier;
    });
    g_assert_false(!!unexpectedRealmDestroyedEvent);
}

static void verifySharedWorkerHostProcessTermination(AutomationTest& test, GRefPtr<WebKitWebView>& webView, const String& browsingContext)
{
    bool wasUsingSeparateWorkerProcess = webkitSetUseSeparateRemoteWorkerProcessForTesting(true);

    test.loadPageAndWaitForTitle(webView.get(), s_workerRealmServer->getURIForPath("/shared-worker-host-termination.html"), "ready");
    auto expectedOrigin = s_workerRealmServer->baseURL().protocolHostAndPort();
    auto realmIdentifier = getSharedWorkerRealmIdentifier(test, 1000, browsingContext, expectedOrigin);

    auto subscriptionParameters = JSON::Object::create();
    auto events = JSON::Array::create();
    events->pushString("script.realmDestroyed"_s);
    subscriptionParameters->setArray("events"_s, WTF::move(events));
    test.sendBidiCommandAndWait(1001, "session.subscribe"_s, WTF::move(subscriptionParameters));

    g_assert_true(webkitWebViewTerminateStandaloneSharedWorkerProcessForTesting(webView.get()));
    webkitSetUseSeparateRemoteWorkerProcessForTesting(wasUsingSeparateWorkerProcess);

    test.waitForBidiMessage([&realmIdentifier](const JSON::Object& message) {
        if (message.getString("method"_s) != "script.realmDestroyed"_s)
            return false;
        auto parameters = message.getObject("params"_s);
        return parameters && parameters->getString("realm"_s) == realmIdentifier;
    });

    auto parameters = JSON::Object::create();
    parameters->setString("type"_s, "shared-worker"_s);
    auto realms = test.getRealms(1002, WTF::move(parameters));
    for (size_t index = 0; index < realms->length(); ++index) {
        auto realm = realms->get(index)->asObject();
        g_assert_true(!realm || realm->getString("realm"_s) != realmIdentifier);
    }

    webView = nullptr;
    waitUntilNoSharedWorkerRealms(test, 1003);
}
#endif

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

#if ENABLE(WEBDRIVER_BIDI)
    s_workerRealmServer = makeUnique<WebKitTestServer>();
    s_workerRealmServer->run(workerRealmServerCallback);
#endif

    AutomationTest::add("WebKitAutomationSession", "request-session", testAutomationSessionRequestSession);
    Test::add("WebKitAutomationSession", "application-info", testAutomationSessionApplicationInfo);
}

void afterAll()
{
#if ENABLE(WEBDRIVER_BIDI)
    s_workerRealmServer = nullptr;
#endif
    g_unsetenv("WEBKIT_INSPECTOR_SERVER");
}
