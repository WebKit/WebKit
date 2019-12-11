/*
 * Copyright (C) 2015 Igalia S.L.
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

#include "WebViewTest.h"

class ConsoleMessageTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ConsoleMessageTest);

    // This should be keep in sync with the public enums in WebKitConsoleMessage.h.
    enum class MessageSource { JavaScript, Network, ConsoleAPI, Security, Other };
    enum class MessageLevel { Info, Log, Warning, Error, Debug };
    struct ConsoleMessage {
        bool operator==(const ConsoleMessage& other)
        {
            return source == other.source
                && level == other.level
                && message == other.message
                && lineNumber == other.lineNumber
                && sourceID == other.sourceID;
        }

        MessageSource source;
        MessageLevel level;
        CString message;
        unsigned lineNumber;
        CString sourceID;
    };

    static void consoleMessageReceivedCallback(WebKitUserContentManager*, WebKitJavascriptResult* message, ConsoleMessageTest* test)
    {
        g_assert_nonnull(message);
        GUniquePtr<char> messageString(WebViewTest::javascriptResultToCString(message));
        GRefPtr<GVariant> variant = g_variant_parse(G_VARIANT_TYPE("(uusus)"), messageString.get(), nullptr, nullptr, nullptr);
        g_assert_nonnull(variant.get());

        unsigned source, level, lineNumber;
        const char* messageText;
        const char* sourceID;
        g_variant_get(variant.get(), "(uu&su&s)", &source, &level, &messageText, &lineNumber, &sourceID);
        test->m_consoleMessage = { static_cast<ConsoleMessageTest::MessageSource>(source), static_cast<ConsoleMessageTest::MessageLevel>(level), messageText, lineNumber, sourceID };

        g_main_loop_quit(test->m_mainLoop);
    }

    ConsoleMessageTest()
    {
        webkit_user_content_manager_register_script_message_handler(m_userContentManager.get(), "console");
        g_signal_connect(m_userContentManager.get(), "script-message-received::console", G_CALLBACK(consoleMessageReceivedCallback), this);
    }

    ~ConsoleMessageTest()
    {
        g_signal_handlers_disconnect_matched(m_userContentManager.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        webkit_user_content_manager_unregister_script_message_handler(m_userContentManager.get(), "console");
    }

    void waitUntilConsoleMessageReceived()
    {
        g_main_loop_run(m_mainLoop);
    }

    ConsoleMessage m_consoleMessage;
};

static void testWebKitConsoleMessageConsoleAPI(ConsoleMessageTest* test, gconstpointer)
{
    ConsoleMessageTest::ConsoleMessage referenceMessage = { ConsoleMessageTest::MessageSource::ConsoleAPI, ConsoleMessageTest::MessageLevel::Log, "Log Console Message", 1, "http://foo.com/bar" };
    test->loadHtml("<html><body onload='console.log(\"Log Console Message\");'></body></html>", "http://foo.com/bar");
    test->waitUntilConsoleMessageReceived();
    g_assert_true(test->m_consoleMessage == referenceMessage);

    referenceMessage.level = ConsoleMessageTest::MessageLevel::Info;
    referenceMessage.message = "Info Console Message";
    test->loadHtml("<html><body onload='console.info(\"Info Console Message\");'></body></html>", "http://foo.com/bar");
    test->waitUntilConsoleMessageReceived();
    g_assert_true(test->m_consoleMessage == referenceMessage);

    referenceMessage.level = ConsoleMessageTest::MessageLevel::Warning;
    referenceMessage.message = "Warning Console Message";
    test->loadHtml("<html><body onload='console.warn(\"Warning Console Message\");'></body></html>", "http://foo.com/bar");
    test->waitUntilConsoleMessageReceived();
    g_assert_true(test->m_consoleMessage == referenceMessage);

    referenceMessage.level = ConsoleMessageTest::MessageLevel::Error;
    referenceMessage.message = "Error Console Message";
    test->loadHtml("<html><body onload='console.error(\"Error Console Message\");'></body></html>", "http://foo.com/bar");
    test->waitUntilConsoleMessageReceived();
    g_assert_true(test->m_consoleMessage == referenceMessage);

    referenceMessage.level = ConsoleMessageTest::MessageLevel::Debug;
    referenceMessage.message = "Debug Console Message";
    test->loadHtml("<html><body onload='console.debug(\"Debug Console Message\");'></body></html>", "http://foo.com/bar");
    test->waitUntilConsoleMessageReceived();
    g_assert_true(test->m_consoleMessage == referenceMessage);
}

static void testWebKitConsoleMessageJavaScriptException(ConsoleMessageTest* test, gconstpointer)
{
    ConsoleMessageTest::ConsoleMessage referenceMessage = { ConsoleMessageTest::MessageSource::JavaScript, ConsoleMessageTest::MessageLevel::Error,
        "ReferenceError: Can't find variable: foo", 1, "http://foo.com/bar" };
    test->loadHtml("<html><body onload='foo()'></body></html>", "http://foo.com/bar");
    test->waitUntilConsoleMessageReceived();
    g_assert_true(test->m_consoleMessage == referenceMessage);
}

static void testWebKitConsoleMessageNetworkError(ConsoleMessageTest* test, gconstpointer)
{
    ConsoleMessageTest::ConsoleMessage referenceMessage = { ConsoleMessageTest::MessageSource::Network, ConsoleMessageTest::MessageLevel::Error,
        "Failed to load resource: The resource at “/org/webkit/glib/tests/not-found.css” does not exist", 0, "resource:///org/webkit/glib/tests/not-found.css" };
    test->loadHtml("<html><head><link rel='stylesheet' href='not-found.css' type='text/css'></head><body></body></html>", "resource:///org/webkit/glib/tests/");
    test->waitUntilConsoleMessageReceived();
    g_assert_true(test->m_consoleMessage == referenceMessage);
}

static void testWebKitConsoleMessageSecurityError(ConsoleMessageTest* test, gconstpointer)
{
    ConsoleMessageTest::ConsoleMessage referenceMessage = { ConsoleMessageTest::MessageSource::Security, ConsoleMessageTest::MessageLevel::Error,
        "Not allowed to load local resource: file:///foo/bar/source.png", 1, "http://foo.com/bar" };
    test->loadHtml("<html><body><img src=\"file:///foo/bar/source.png\"/></body></html>", "http://foo.com/bar");
    test->waitUntilConsoleMessageReceived();
    g_assert_true(test->m_consoleMessage == referenceMessage);
}

void beforeAll()
{
    ConsoleMessageTest::add("WebKitConsoleMessage", "console-api", testWebKitConsoleMessageConsoleAPI);
    ConsoleMessageTest::add("WebKitConsoleMessage", "js-exception", testWebKitConsoleMessageJavaScriptException);
    ConsoleMessageTest::add("WebKitConsoleMessage", "network-error", testWebKitConsoleMessageNetworkError);
    ConsoleMessageTest::add("WebKitConsoleMessage", "security-error", testWebKitConsoleMessageSecurityError);
}

void afterAll()
{
}
