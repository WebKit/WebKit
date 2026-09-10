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
#include "IdentifierTypes.h"
#include "WebKitTestServer.h"
#endif
#include <gio/gio.h>
#if ENABLE(WEBDRIVER_BIDI)
#include <wtf/HashMap.h>
#include <wtf/JSONValues.h>
#endif
#include <wtf/UUID.h>
#if ENABLE(WEBDRIVER_BIDI)
#include <wtf/URL.h>
#include <wtf/Vector.h>
#endif
#include <wtf/glib/GLibExtras.h>
#include <wtf/glib/SocketConnection.h>
#if ENABLE(WEBDRIVER_BIDI)
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#endif
#include <wtf/text/StringBuilder.h>
#include <wtf/text/UTF8CStringView.h>

#if ENABLE(WEBDRIVER_BIDI)
static std::unique_ptr<WebKitTestServer> s_iframeRealmTestServer;

static void iframeRealmTestServerCallback(SoupServer*, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
{
    static constexpr auto enumerationParentDocument = "<!doctype html><iframe src='/iframe.html'></iframe>";
    static constexpr auto lifecycleParentDocument = "<!doctype html><iframe src='/initial-iframe.html'></iframe>";
    static constexpr auto subscriptionParentDocument = "<!doctype html><iframe srcdoc='<p>iframe document</p>'></iframe>";
    static constexpr auto iframeDocument = "<!doctype html><script>window.realmIsReady = true;</script><p>iframe document</p>";
    static constexpr auto replacementParentDocument = "<!doctype html><p>replacement parent document</p>";

    const char* content;
    if (g_str_equal(path, "/iframe-realm-enumeration.html"))
        content = enumerationParentDocument;
    else if (g_str_equal(path, "/iframe-realm-lifecycle.html"))
        content = lifecycleParentDocument;
    else if (g_str_equal(path, "/iframe-realm-subscription.html"))
        content = subscriptionParentDocument;
    else if (g_str_equal(path, "/iframe.html") || g_str_equal(path, "/initial-iframe.html") || g_str_equal(path, "/replacement-iframe.html"))
        content = iframeDocument;
    else if (g_str_equal(path, "/replacement-parent.html"))
        content = replacementParentDocument;
    else {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
        return;
    }

    soup_server_message_set_response(message, "text/html", SOUP_MEMORY_STATIC, content, strlen(content));
    soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
}

static void enableSiteIsolation(WebKitWebView* webView)
{
    auto* allFeatures = webkit_settings_get_all_features();
    auto* siteIsolationFeature = webkit_feature_list_find(allFeatures, "SiteIsolation");
    g_assert_nonnull(siteIsolationFeature);
    webkit_settings_set_feature_enabled(webkit_web_view_get_settings(webView), siteIsolationFeature, TRUE);
    webkit_feature_list_unref(allFeatures);
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

    ~AutomationTest()
    {
        if (m_session && m_connection && !m_connection->isClosed() && m_target.isPaired) {
            sendCommandToBackend("deleteSession"_s);
            g_main_loop_run(m_mainLoop.get());
        }
        if (m_connection)
            m_connection->close();
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
        m_message = UTF8CString::unsafeFromUTF8(message);
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
        m_connection->sendMessage("SendMessageToBackend"_s, gVariantNew("(tts)", m_connectionID, m_target.id, messageBuilder.toString().utf8()));
    }

#if ENABLE(WEBDRIVER_BIDI)
    String browsingContextHandleFromLastResponse() const
    {
        auto responseValue = JSON::Value::parseJSON(String::fromUTF8(m_message.span()));
        g_assert_true(!!responseValue);
        auto response = responseValue->asObject();
        g_assert_true(!!response);
        auto result = response->getObject("result"_s);
        g_assert_true(!!result);
        auto browsingContext = result->getString("handle"_s);
        g_assert_false(browsingContext.isEmpty());
        return browsingContext;
    }

    void loadURIAndWait(WebKitWebView* webView, const char* uri)
    {
        m_loadFinished = false;
        auto loadChangedHandler = g_signal_connect(webView, "load-changed", G_CALLBACK(+[](WebKitWebView*, WebKitLoadEvent loadEvent, AutomationTest* test) {
            if (loadEvent != WEBKIT_LOAD_FINISHED)
                return;
            test->m_loadFinished = true;
            g_main_loop_quit(test->m_mainLoop.get());
        }), this);

        webkit_web_view_load_uri(webView, uri);
        while (!m_loadFinished)
            g_main_loop_run(m_mainLoop.get());
        g_signal_handler_disconnect(webView, loadChangedHandler);
    }

    RefPtr<JSON::Object> sendBidiCommandAndWaitForResponse(int commandIdentifier, const String& method, Ref<JSON::Object>&& parameters)
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
        return response;
    }

    Ref<JSON::Object> sendBidiCommandAndWait(int commandIdentifier, const String& method, Ref<JSON::Object>&& parameters)
    {
        auto response = sendBidiCommandAndWaitForResponse(commandIdentifier, method, WTF::move(parameters));
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
            g_test_message("Unmatched BiDi message: %s", serializedMessage.legacyCStringPointer());
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
        g_signal_connect(session, "will-close", G_CALLBACK(+[](WebKitAutomationSession*, AutomationTest* test) {
            test->m_session = nullptr;
            g_main_loop_quit(test->m_mainLoop.get());
        }), this);
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
    WebKitAutomationSession* m_session { nullptr };
    guint64 m_connectionID { 0 };
    Target m_target;

    WebKitWebView* m_webViewForAutomation { nullptr };
    bool m_createWebViewWasCalled { false };
    bool m_createWebViewInWindowWasCalled { false };
    bool m_createWebViewInTabWasCalled { false };
    UTF8CString m_message;
#if ENABLE(WEBDRIVER_BIDI)
    bool m_loadFinished { false };
    Vector<Ref<JSON::Object>> m_bidiMessages;
#endif
};

const SocketConnection::MessageHandlers AutomationTest::s_messageHandlers = {
    { "DidClose"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { { },
        [](SocketConnection&, GVariant*, gpointer userData) {
            auto& test = *static_cast<AutomationTest*>(userData);
            test.m_connection = nullptr;
        } }
    },
    { "DidStartAutomationSession"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { "(ss)"_s,
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& test = *static_cast<AutomationTest*>(userData);
            test.didStartAutomationSession(parameters);
        } }
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
        } }
    },
    { "SendMessageToFrontend"_s, std::pair<ASCIICString, SocketConnection::MessageCallback> { "(tts)"_s,
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
struct IframeRealm {
    String identifier;
    String browsingContext;
};

struct BidiTestBrowsingContext {
    GRefPtr<WebKitWebView> webView;
    String topLevelBrowsingContext;
};

static BidiTestBrowsingContext createBidiTestBrowsingContextInCurrentSession(AutomationTest& test, bool siteIsolationEnabled = false)
{
    auto webView = test.createWebView("is-controlled-by-automation", TRUE, nullptr);
    g_assert_true(webkit_web_view_is_controlled_by_automation(webView.get()));
    if (siteIsolationEnabled)
        enableSiteIsolation(webView.get());
    g_assert_true(test.createTopLevelBrowsingContext(webView.get()));

    auto topLevelBrowsingContext = test.browsingContextHandleFromLastResponse();
    return { WTF::move(webView), WTF::move(topLevelBrowsingContext) };
}

static BidiTestBrowsingContext createBidiTestBrowsingContext(AutomationTest& test, bool siteIsolationEnabled = false)
{
    webkit_web_context_set_automation_allowed(test.m_webContext.get(), TRUE);
    auto sessionID = createVersion4UUIDString().utf8();
    g_assert_nonnull(test.requestSession(sessionID));
    return createBidiTestBrowsingContextInCurrentSession(test, siteIsolationEnabled);
}

struct ProtocolRealmIdentifierComponents {
    uint64_t processIdentifier;
    uint64_t localIdentifier;
};

static std::optional<ProtocolRealmIdentifierComponents> parseProtocolRealmIdentifier(const String& realmIdentifier)
{
    if (!realmIdentifier.startsWith("realm-"_s))
        return std::nullopt;

    auto serializedIdentifier = realmIdentifier.substring(6);
    auto separatorPosition = serializedIdentifier.find('-');
    if (separatorPosition == notFound || !separatorPosition || separatorPosition == serializedIdentifier.length() - 1
        || serializedIdentifier.find('-', separatorPosition + 1) != notFound)
        return std::nullopt;

    auto processIdentifier = parseInteger<uint64_t>(serializedIdentifier.left(separatorPosition));
    auto localIdentifier = parseInteger<uint64_t>(serializedIdentifier.substring(separatorPosition + 1));
    if (!processIdentifier || !*processIdentifier || !localIdentifier || !*localIdentifier)
        return std::nullopt;

    return ProtocolRealmIdentifierComponents { *processIdentifier, *localIdentifier };
}

static void verifyProcessQualifiedRealmIdentifierIsolation()
{
    WebKit::NonProcessQualifiedRealmIdentifier sharedLocalIdentifier { 1 };
    WebKit::RealmIdentifier oldRealmIdentifier { sharedLocalIdentifier, WebCore::ProcessIdentifier { 1 } };
    WebKit::RealmIdentifier replacementRealmIdentifier { sharedLocalIdentifier, WebCore::ProcessIdentifier { 2 } };

    g_assert_true(oldRealmIdentifier != replacementRealmIdentifier);
    g_assert_true(oldRealmIdentifier.loggingString() != replacementRealmIdentifier.loggingString());

    HashMap<WebKit::RealmIdentifier, unsigned> activeRealms;
    activeRealms.set(oldRealmIdentifier, 1);
    activeRealms.set(replacementRealmIdentifier, 2);
    g_assert_cmpuint(activeRealms.size(), ==, 2);

    activeRealms.remove(oldRealmIdentifier);
    g_assert_false(activeRealms.contains(oldRealmIdentifier));
    g_assert_true(activeRealms.contains(replacementRealmIdentifier));
    g_assert_cmpuint(activeRealms.get(replacementRealmIdentifier), ==, 2);
}

static Ref<JSON::Array> getRealms(AutomationTest& test, int commandIdentifier, Ref<JSON::Object>&& parameters = JSON::Object::create())
{
    auto response = test.sendBidiCommandAndWait(commandIdentifier, "script.getRealms"_s, WTF::move(parameters));
    auto result = response->getObject("result"_s);
    g_assert_true(!!result);
    auto realms = result->getArray("realms"_s);
    g_assert_true(!!realms);
    return realms.releaseNonNull();
}

static String getIframeBrowsingContext(AutomationTest& test, int commandIdentifier, const String& topLevelBrowsingContext)
{
    auto parameters = JSON::Object::create();
    parameters->setString("root"_s, topLevelBrowsingContext);
    auto response = test.sendBidiCommandAndWait(commandIdentifier, "browsingContext.getTree"_s, WTF::move(parameters));
    auto result = response->getObject("result"_s);
    g_assert_true(!!result);
    auto contexts = result->getArray("contexts"_s);
    g_assert_true(!!contexts);
    g_assert_cmpuint(contexts->length(), ==, 1);
    auto topLevelContext = contexts->get(0)->asObject();
    g_assert_true(!!topLevelContext);
    g_assert_true(topLevelContext->getString("context"_s) == topLevelBrowsingContext);
    auto children = topLevelContext->getArray("children"_s);
    g_assert_true(!!children);
    g_assert_cmpuint(children->length(), ==, 1);
    auto iframeContext = children->get(0)->asObject();
    g_assert_true(!!iframeContext);
    auto iframeBrowsingContext = iframeContext->getString("context"_s);
    g_assert_false(iframeBrowsingContext.isEmpty());
    return iframeBrowsingContext;
}

static RefPtr<JSON::Object> disownRealm(AutomationTest& test, int commandIdentifier, const String& realmIdentifier)
{
    auto parameters = JSON::Object::create();
    parameters->setArray("handles"_s, JSON::Array::create());
    auto target = JSON::Object::create();
    target->setString("realm"_s, realmIdentifier);
    parameters->setObject("target"_s, WTF::move(target));
    return test.sendBidiCommandAndWaitForResponse(commandIdentifier, "script.disown"_s, WTF::move(parameters));
}

static void subscribeToRealmEvents(AutomationTest& test, int commandIdentifier, const String& browsingContext = { })
{
    auto parameters = JSON::Object::create();
    auto events = JSON::Array::create();
    events->pushString("script.realmCreated"_s);
    events->pushString("script.realmDestroyed"_s);
    parameters->setArray("events"_s, WTF::move(events));
    if (!browsingContext.isNull()) {
        auto contexts = JSON::Array::create();
        contexts->pushString(browsingContext);
        parameters->setArray("contexts"_s, WTF::move(contexts));
    }
    test.sendBidiCommandAndWait(commandIdentifier, "session.subscribe"_s, WTF::move(parameters));
}

static void evaluateInContext(AutomationTest& test, int commandIdentifier, const String& expression, const String& browsingContext)
{
    auto parameters = JSON::Object::create();
    parameters->setString("expression"_s, expression);
    parameters->setBoolean("awaitPromise"_s, false);
    auto target = JSON::Object::create();
    target->setString("context"_s, browsingContext);
    parameters->setObject("target"_s, WTF::move(target));
    test.sendBidiCommandAndWait(commandIdentifier, "script.evaluate"_s, WTF::move(parameters));
}

static void verifyIframeRealmEnumeration(AutomationTest& test, WebKitWebView* webView, const String& topLevelBrowsingContext)
{
    auto pageURI = s_iframeRealmTestServer->getURIForPath("/iframe-realm-enumeration.html");
    auto expectedOrigin = URL { String::fromUTF8(pageURI.span()) }.protocolHostAndPort();
    test.loadURIAndWait(webView, pageURI.legacyCStringPointer());

    auto iframeBrowsingContext = getIframeBrowsingContext(test, 1, topLevelBrowsingContext);
    // No script command has run in the iframe, so getRealms must create its normal-world realm.
    auto realms = getRealms(test, 2);
    g_assert_cmpuint(realms->length(), ==, 2);

    bool foundTopLevelRealm = false;
    std::optional<IframeRealm> iframeRealm;
    for (size_t index = 0; index < realms->length(); ++index) {
        auto realm = realms->get(index)->asObject();
        g_assert_true(!!realm);
        g_assert_true(realm->getString("type"_s) == "window"_s);
        g_assert_true(realm->getString("origin"_s) == expectedOrigin);

        auto browsingContext = realm->getString("context"_s);
        if (browsingContext == topLevelBrowsingContext) {
            foundTopLevelRealm = true;
            continue;
        }

        g_assert_false(!!iframeRealm);
        iframeRealm = IframeRealm { realm->getString("realm"_s), WTF::move(browsingContext) };
    }

    g_assert_true(foundTopLevelRealm);
    g_assert_true(!!iframeRealm);
    g_assert_false(iframeRealm->identifier.isEmpty());
    g_assert_false(iframeRealm->browsingContext.isEmpty());
    g_assert_true(iframeRealm->browsingContext == iframeBrowsingContext);

    auto filterParameters = JSON::Object::create();
    filterParameters->setString("context"_s, iframeRealm->browsingContext);
    auto filteredRealms = getRealms(test, 3, WTF::move(filterParameters));
    g_assert_cmpuint(filteredRealms->length(), ==, 1);
    auto filteredRealm = filteredRealms->get(0)->asObject();
    g_assert_true(!!filteredRealm);
    g_assert_true(filteredRealm->getString("realm"_s) == iframeRealm->identifier);
    g_assert_true(filteredRealm->getString("context"_s) == iframeRealm->browsingContext);

    auto evaluateParameters = JSON::Object::create();
    evaluateParameters->setString("expression"_s, "1 + 2"_s);
    evaluateParameters->setBoolean("awaitPromise"_s, false);
    auto evaluateTarget = JSON::Object::create();
    evaluateTarget->setString("context"_s, iframeRealm->browsingContext);
    evaluateParameters->setObject("target"_s, WTF::move(evaluateTarget));
    auto evaluateResponse = test.sendBidiCommandAndWait(4, "script.evaluate"_s, WTF::move(evaluateParameters));
    auto evaluateResult = evaluateResponse->getObject("result"_s);
    g_assert_true(!!evaluateResult);
    // getRealms must bind the reported realm to the current iframe handle, even if top-level navigation invalidated an earlier handle.
    g_assert_true(evaluateResult->getString("realm"_s) == iframeRealm->identifier);

    // Use a new scriptless iframe so callFunction, rather than getRealms, creates this realm.
    test.loadURIAndWait(webView, pageURI.legacyCStringPointer());
    auto callFunctionIframeBrowsingContext = getIframeBrowsingContext(test, 5, topLevelBrowsingContext);
    g_assert_true(callFunctionIframeBrowsingContext != iframeBrowsingContext);

    auto callFunctionParameters = JSON::Object::create();
    callFunctionParameters->setString("functionDeclaration"_s, "() => 1"_s);
    callFunctionParameters->setBoolean("awaitPromise"_s, false);
    auto callFunctionTarget = JSON::Object::create();
    callFunctionTarget->setString("context"_s, callFunctionIframeBrowsingContext);
    callFunctionParameters->setObject("target"_s, WTF::move(callFunctionTarget));
    auto callFunctionResponse = test.sendBidiCommandAndWait(6, "script.callFunction"_s, WTF::move(callFunctionParameters));
    auto callFunctionResult = callFunctionResponse->getObject("result"_s);
    g_assert_true(!!callFunctionResult);
    auto callFunctionRealm = callFunctionResult->getString("realm"_s);
    g_assert_false(callFunctionRealm.isEmpty());

    auto callFunctionFilterParameters = JSON::Object::create();
    callFunctionFilterParameters->setString("context"_s, callFunctionIframeBrowsingContext);
    auto callFunctionRealms = getRealms(test, 7, WTF::move(callFunctionFilterParameters));
    g_assert_cmpuint(callFunctionRealms->length(), ==, 1);
    auto callFunctionRealmInfo = callFunctionRealms->get(0)->asObject();
    g_assert_true(!!callFunctionRealmInfo);
    g_assert_true(callFunctionRealmInfo->getString("realm"_s) == callFunctionRealm);
    g_assert_true(callFunctionRealmInfo->getString("context"_s) == callFunctionIframeBrowsingContext);
    g_assert_true(callFunctionRealmInfo->getString("origin"_s) == expectedOrigin);
}

static IframeRealm getIframeRealm(AutomationTest& test, int commandIdentifier, const String& topLevelBrowsingContext)
{
    auto realms = getRealms(test, commandIdentifier);
    std::optional<IframeRealm> iframeRealm;
    for (size_t index = 0; index < realms->length(); ++index) {
        auto realm = realms->get(index)->asObject();
        if (!realm || realm->getString("type"_s) != "window"_s)
            continue;
        auto browsingContext = realm->getString("context"_s);
        if (browsingContext == topLevelBrowsingContext)
            continue;
        g_assert_false(!!iframeRealm);
        iframeRealm = IframeRealm { realm->getString("realm"_s), WTF::move(browsingContext) };
    }
    g_assert_true(!!iframeRealm);
    g_assert_false(iframeRealm->identifier.isEmpty());
    return WTF::move(*iframeRealm);
}

static bool isRealmEvent(const JSON::Object& message, ASCIILiteral method, const String& realmIdentifier)
{
    if (message.getString("method"_s) != method)
        return false;
    auto parameters = message.getObject("params"_s);
    return parameters && parameters->getString("realm"_s) == realmIdentifier;
}

static void waitForRealmDestroyed(AutomationTest& test, const String& realmIdentifier)
{
    auto message = test.waitForBidiMessage([&](const JSON::Object& message) {
        return isRealmEvent(message, "script.realmDestroyed"_s, realmIdentifier);
    });
    g_assert_true(!!message);
    auto parameters = message->getObject("params"_s);
    g_assert_true(!!parameters);
    g_assert_false(!!parameters->getValue("context"_s));
}

static String waitForReplacementRealmCreated(AutomationTest& test, const IframeRealm& previousRealm)
{
    auto message = test.waitForBidiMessage([&](const JSON::Object& message) {
        if (message.getString("method"_s) != "script.realmCreated"_s)
            return false;
        auto parameters = message.getObject("params"_s);
        return parameters && parameters->getString("context"_s) == previousRealm.browsingContext
            && parameters->getString("realm"_s) != previousRealm.identifier;
    });
    g_assert_true(!!message);
    auto parameters = message->getObject("params"_s);
    g_assert_true(!!parameters);
    g_assert_true(parameters->getString("type"_s) == "window"_s);
    auto realmIdentifier = parameters->getString("realm"_s);
    g_assert_false(realmIdentifier.isEmpty());
    return realmIdentifier;
}

static void assertRealmWasDestroyedOnce(AutomationTest& test, const String& realmIdentifier)
{
    g_assert_false(!!test.takeBidiMessage([&](const JSON::Object& message) {
        return isRealmEvent(message, "script.realmDestroyed"_s, realmIdentifier);
    }));
}

static Ref<JSON::Array> getRealmsForContext(AutomationTest& test, int commandIdentifier, const String& browsingContext)
{
    auto parameters = JSON::Object::create();
    parameters->setString("context"_s, browsingContext);
    return getRealms(test, commandIdentifier, WTF::move(parameters));
}

static String getRealmForContext(AutomationTest& test, int commandIdentifier, const String& browsingContext)
{
    auto realms = getRealmsForContext(test, commandIdentifier, browsingContext);
    g_assert_cmpuint(realms->length(), ==, 1);
    auto realm = realms->get(0)->asObject();
    g_assert_true(!!realm);
    g_assert_true(realm->getString("context"_s) == browsingContext);
    auto realmIdentifier = realm->getString("realm"_s);
    g_assert_false(realmIdentifier.isEmpty());
    return realmIdentifier;
}

static void waitForRealmCreated(AutomationTest& test, const String& realmIdentifier, const String& browsingContext)
{
    auto message = test.waitForBidiMessage([&](const JSON::Object& message) {
        return isRealmEvent(message, "script.realmCreated"_s, realmIdentifier);
    });
    g_assert_true(!!message);
    auto parameters = message->getObject("params"_s);
    g_assert_true(!!parameters);
    g_assert_true(parameters->getString("type"_s) == "window"_s);
    g_assert_true(parameters->getString("context"_s) == browsingContext);
}

static void assertCurrentIframeRealm(AutomationTest& test, int commandIdentifier, const IframeRealm& previousRealm, const String& expectedRealmIdentifier)
{
    auto realms = getRealmsForContext(test, commandIdentifier, previousRealm.browsingContext);
    g_assert_cmpuint(realms->length(), ==, 1);
    auto realm = realms->get(0)->asObject();
    g_assert_true(!!realm);
    g_assert_true(realm->getString("realm"_s) == expectedRealmIdentifier);
    assertRealmWasDestroyedOnce(test, previousRealm.identifier);
}

static void verifyIframeRealmLifecycle(AutomationTest& test, WebKitWebView* webView, const String& topLevelBrowsingContext)
{
    verifyProcessQualifiedRealmIdentifierIsolation();

    test.loadURIAndWait(webView, s_iframeRealmTestServer->getURIForPath("/iframe-realm-lifecycle.html").legacyCStringPointer());
    auto initialIframeRealm = getIframeRealm(test, 4, topLevelBrowsingContext);
    auto initialRealmIdentifier = parseProtocolRealmIdentifier(initialIframeRealm.identifier);
    g_assert_true(!!initialRealmIdentifier);

    subscribeToRealmEvents(test, 5);

    auto crossSiteIframeURL = makeString("http://localhost:"_s, s_iframeRealmTestServer->port(), "/replacement-iframe.html"_s);
    evaluateInContext(test, 6, makeString("document.querySelector('iframe').src = \""_s, crossSiteIframeURL, "\";"_s), topLevelBrowsingContext);

    waitForRealmDestroyed(test, initialIframeRealm.identifier);
    auto replacementIframeRealmIdentifier = waitForReplacementRealmCreated(test, initialIframeRealm);
    auto replacementRealmIdentifier = parseProtocolRealmIdentifier(replacementIframeRealmIdentifier);
    g_assert_true(!!replacementRealmIdentifier);
    g_assert_true(replacementRealmIdentifier->processIdentifier != initialRealmIdentifier->processIdentifier);
    assertCurrentIframeRealm(test, 7, initialIframeRealm, replacementIframeRealmIdentifier);

    test.loadURIAndWait(webView, s_iframeRealmTestServer->getURIForPath("/replacement-parent.html").legacyCStringPointer());
    waitForRealmDestroyed(test, replacementIframeRealmIdentifier);

    auto activeRealms = getRealms(test, 8);
    for (size_t index = 0; index < activeRealms->length(); ++index) {
        auto realm = activeRealms->get(index)->asObject();
        if (!realm)
            continue;
        auto realmIdentifier = realm->getString("realm"_s);
        g_assert_true(realmIdentifier != initialIframeRealm.identifier);
        g_assert_true(realmIdentifier != replacementIframeRealmIdentifier);
    }
    assertRealmWasDestroyedOnce(test, initialIframeRealm.identifier);
    assertRealmWasDestroyedOnce(test, replacementIframeRealmIdentifier);
}

static void testBidiIframeRealmEnumeration(AutomationTest* test, gconstpointer)
{
    auto context = createBidiTestBrowsingContext(*test);
    verifyIframeRealmEnumeration(*test, context.webView.get(), context.topLevelBrowsingContext);
}

static void testBidiRealmTargetParser(AutomationTest* test, gconstpointer)
{
    auto context = createBidiTestBrowsingContext(*test);
    auto realm = getRealmForContext(*test, 1, context.topLevelBrowsingContext);

    auto validTargetResponse = disownRealm(*test, 2, realm);
    g_assert_true(validTargetResponse->getString("type"_s) == "error"_s);
    g_assert_true(validTargetResponse->getString("error"_s) == "unsupported operation"_s);

    int commandIdentifier = 3;
    for (auto invalidRealmIdentifier : { "realm-5"_s, "realm-1-2-3"_s, "realm-0-1"_s }) {
        auto invalidTargetResponse = disownRealm(*test, commandIdentifier++, invalidRealmIdentifier);
        g_assert_true(invalidTargetResponse->getString("type"_s) == "error"_s);
        g_assert_true(invalidTargetResponse->getString("error"_s) == "no such frame"_s);
    }
}

static void testBidiTopLevelContextSubscriptionIncludesIframeRealms(AutomationTest* test, gconstpointer)
{
    auto subscribedContext = createBidiTestBrowsingContext(*test);
    test->loadURIAndWait(subscribedContext.webView.get(), s_iframeRealmTestServer->getURIForPath("/iframe-realm-subscription.html").legacyCStringPointer());
    auto iframeRealm = getIframeRealm(*test, 1, subscribedContext.topLevelBrowsingContext);
    auto topLevelRealm = getRealmForContext(*test, 2, subscribedContext.topLevelBrowsingContext);

    auto excludedContext = createBidiTestBrowsingContextInCurrentSession(*test);
    test->loadURIAndWait(excludedContext.webView.get(), s_iframeRealmTestServer->getURIForPath("/iframe-realm-subscription.html").legacyCStringPointer());
    auto excludedTopLevelRealm = getRealmForContext(*test, 3, excludedContext.topLevelBrowsingContext);

    subscribeToRealmEvents(*test, 4, subscribedContext.topLevelBrowsingContext);
    waitForRealmCreated(*test, topLevelRealm, subscribedContext.topLevelBrowsingContext);
    waitForRealmCreated(*test, iframeRealm.identifier, iframeRealm.browsingContext);

    g_assert_false(!!test->takeBidiMessage([&](const JSON::Object& message) {
        return isRealmEvent(message, "script.realmCreated"_s, excludedTopLevelRealm);
    }));
    g_assert_false(!!test->takeBidiMessage([](const JSON::Object& message) {
        return message.getString("method"_s) == "script.realmCreated"_s;
    }));
}

static void testBidiIframeRealmCrossProcessNavigation(AutomationTest* test, gconstpointer)
{
    auto context = createBidiTestBrowsingContext(*test, true);
    verifyIframeRealmLifecycle(*test, context.webView.get(), context.topLevelBrowsingContext);
}

static void testBidiIframeRealmSameProcessNavigation(AutomationTest* test, gconstpointer)
{
    auto context = createBidiTestBrowsingContext(*test);
    test->loadURIAndWait(context.webView.get(), s_iframeRealmTestServer->getURIForPath("/iframe-realm-lifecycle.html").legacyCStringPointer());
    auto initialRealm = getIframeRealm(*test, 1, context.topLevelBrowsingContext);
    auto initialRealmIdentifier = parseProtocolRealmIdentifier(initialRealm.identifier);
    g_assert_true(!!initialRealmIdentifier);

    subscribeToRealmEvents(*test, 2);
    evaluateInContext(*test, 3, "document.querySelector('iframe').src = '/replacement-iframe.html';"_s, context.topLevelBrowsingContext);

    waitForRealmDestroyed(*test, initialRealm.identifier);
    auto replacementRealm = waitForReplacementRealmCreated(*test, initialRealm);
    auto replacementRealmIdentifier = parseProtocolRealmIdentifier(replacementRealm);
    g_assert_true(!!replacementRealmIdentifier);
    g_assert_true(replacementRealmIdentifier->processIdentifier == initialRealmIdentifier->processIdentifier);
    assertCurrentIframeRealm(*test, 4, initialRealm, replacementRealm);
}

static void testBidiIframeRealmRemoval(AutomationTest* test, gconstpointer)
{
    auto context = createBidiTestBrowsingContext(*test);
    test->loadURIAndWait(context.webView.get(), s_iframeRealmTestServer->getURIForPath("/iframe-realm-lifecycle.html").legacyCStringPointer());
    auto iframeRealm = getIframeRealm(*test, 1, context.topLevelBrowsingContext);

    subscribeToRealmEvents(*test, 2);
    evaluateInContext(*test, 3, "document.querySelector('iframe').remove();"_s, context.topLevelBrowsingContext);
    waitForRealmDestroyed(*test, iframeRealm.identifier);

    auto activeRealms = getRealms(*test, 4);
    for (size_t index = 0; index < activeRealms->length(); ++index) {
        auto realm = activeRealms->get(index)->asObject();
        g_assert_true(!!realm);
        g_assert_true(realm->getString("realm"_s) != iframeRealm.identifier);
        g_assert_true(realm->getString("context"_s) != iframeRealm.browsingContext);
    }
    assertRealmWasDestroyedOnce(*test, iframeRealm.identifier);
}

static void testBidiPageCloseDestroysAllRealms(AutomationTest* test, gconstpointer)
{
    auto context = createBidiTestBrowsingContext(*test);
    test->loadURIAndWait(context.webView.get(), s_iframeRealmTestServer->getURIForPath("/iframe-realm-lifecycle.html").legacyCStringPointer());
    auto topLevelRealm = getRealmForContext(*test, 1, context.topLevelBrowsingContext);
    auto iframeRealm = getIframeRealm(*test, 2, context.topLevelBrowsingContext);

    g_signal_connect(context.webView.get(), "close", G_CALLBACK(+[](WebKitWebView* webView, gpointer) {
#if PLATFORM(GTK) && !USE(GTK4)
        gtk_widget_destroy(GTK_WIDGET(webView));
#else
        g_object_run_dispose(G_OBJECT(webView));
#endif
    }), nullptr);

    subscribeToRealmEvents(*test, 3);
    auto closeParameters = JSON::Object::create();
    closeParameters->setString("context"_s, context.topLevelBrowsingContext);
    test->sendBidiCommandAndWait(4, "browsingContext.close"_s, WTF::move(closeParameters));

    waitForRealmDestroyed(*test, topLevelRealm);
    waitForRealmDestroyed(*test, iframeRealm.identifier);
    auto activeRealms = getRealms(*test, 5);
    g_assert_cmpuint(activeRealms->length(), ==, 0);
    assertRealmWasDestroyedOnce(*test, topLevelRealm);
    assertRealmWasDestroyedOnce(*test, iframeRealm.identifier);
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

#if ENABLE(WEBDRIVER_BIDI)
    s_iframeRealmTestServer = makeUnique<WebKitTestServer>();
    s_iframeRealmTestServer->run(iframeRealmTestServerCallback);
    AutomationTest::add("WebKitAutomationSession", "bidi-iframe-realm-enumeration", testBidiIframeRealmEnumeration);
    AutomationTest::add("WebKitAutomationSession", "bidi-realm-target-parser", testBidiRealmTargetParser);
    AutomationTest::add("WebKitAutomationSession", "bidi-top-level-context-subscription-includes-iframe-realms", testBidiTopLevelContextSubscriptionIncludesIframeRealms);
    AutomationTest::add("WebKitAutomationSession", "bidi-iframe-realm-cross-process-navigation", testBidiIframeRealmCrossProcessNavigation);
    AutomationTest::add("WebKitAutomationSession", "bidi-iframe-realm-same-process-navigation", testBidiIframeRealmSameProcessNavigation);
    AutomationTest::add("WebKitAutomationSession", "bidi-iframe-realm-removal", testBidiIframeRealmRemoval);
    AutomationTest::add("WebKitAutomationSession", "bidi-page-close-destroys-all-realms", testBidiPageCloseDestroysAllRealms);
#endif

    AutomationTest::add("WebKitAutomationSession", "request-session", testAutomationSessionRequestSession);
    Test::add("WebKitAutomationSession", "application-info", testAutomationSessionApplicationInfo);
}

void afterAll()
{
#if ENABLE(WEBDRIVER_BIDI)
    s_iframeRealmTestServer = nullptr;
#endif
    g_unsetenv("WEBKIT_INSPECTOR_SERVER");
}
