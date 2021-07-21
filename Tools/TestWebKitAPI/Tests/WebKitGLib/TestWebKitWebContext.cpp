/*
 * Copyright (C) 2011 Igalia S.L.
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

#include "LoadTrackingTest.h"
#include "WebKitTestServer.h"
#include <WebCore/SoupVersioning.h>
#include <limits.h>
#include <stdlib.h>
#include <wtf/HashMap.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

static WebKitTestServer* kServer;

static void testWebContextDefault(Test* test, gconstpointer)
{
    // Check there's a single instance of the default web context.
    g_assert_true(webkit_web_context_get_default() == webkit_web_context_get_default());
    g_assert_true(webkit_web_context_get_default() != test->m_webContext.get());
}

static void testWebContextEphemeral(Test* test, gconstpointer)
{
    // By default web contexts are not ephemeral.
    g_assert_false(webkit_web_context_is_ephemeral(webkit_web_context_get_default()));
    g_assert_false(webkit_web_context_is_ephemeral(test->m_webContext.get()));

    WebKitWebsiteDataManager* manager = webkit_web_context_get_website_data_manager(webkit_web_context_get_default());
    g_assert_true(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager));
    g_assert_false(webkit_website_data_manager_is_ephemeral(manager));
    manager = webkit_web_context_get_website_data_manager(test->m_webContext.get());
    g_assert_true(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager));
    g_assert_false(webkit_website_data_manager_is_ephemeral(manager));

    auto webView = Test::adoptView(Test::createWebView());
    g_assert_false(webkit_web_view_is_ephemeral(webView.get()));
    g_assert_true(webkit_web_view_get_website_data_manager(webView.get()) == webkit_web_context_get_website_data_manager(webkit_web_context_get_default()));

    webView = Test::adoptView(Test::createWebView(test->m_webContext.get()));
    g_assert_false(webkit_web_view_is_ephemeral(webView.get()));
    g_assert_true(webkit_web_view_get_website_data_manager(webView.get()) == manager);

    GRefPtr<WebKitWebContext> context = adoptGRef(webkit_web_context_new_ephemeral());
    g_assert_true(webkit_web_context_is_ephemeral(context.get()));
    manager = webkit_web_context_get_website_data_manager(context.get());
    g_assert_true(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager));
    g_assert_true(webkit_website_data_manager_is_ephemeral(manager));
    g_assert_true(webkit_web_view_get_website_data_manager(webView.get()) != manager);

    webView = Test::adoptView(Test::createWebView(context.get()));
    g_assert_true(webkit_web_view_is_ephemeral(webView.get()));
    g_assert_true(webkit_web_view_get_website_data_manager(webView.get()) == manager);

    GRefPtr<WebKitWebsiteDataManager> ephemeralManager = adoptGRef(webkit_website_data_manager_new_ephemeral());
    g_assert_true(webkit_website_data_manager_is_ephemeral(ephemeralManager.get()));
    context = adoptGRef(webkit_web_context_new_with_website_data_manager(ephemeralManager.get()));
    g_assert_true(webkit_web_context_is_ephemeral(context.get()));
}

static const char* kBarHTML = "<html><body>Bar</body></html>";
static const char* kEchoHTMLFormat = "<html><body>%s</body></html>";
static const char* errorDomain = "test";
static const int errorCode = 10;

static const char* genericErrorMessage = "Error message.";
static const char* beforeReceiveResponseErrorMessage = "Error before didReceiveResponse.";

class URISchemeTest: public LoadTrackingTest {
public:
    MAKE_GLIB_TEST_FIXTURE(URISchemeTest);

    struct URISchemeHandler {
        URISchemeHandler()
            : replyLength(0)
        {
        }

        URISchemeHandler(const char* reply, int replyLength, const char* mimeType)
            : reply(reply)
            , replyLength(replyLength)
            , mimeType(mimeType)
        {
        }

        CString reply;
        int replyLength;
        CString mimeType;
    };

    static void uriSchemeRequestCallback(WebKitURISchemeRequest* request, gpointer userData)
    {
        URISchemeTest* test = static_cast<URISchemeTest*>(userData);
        test->m_uriSchemeRequest = request;
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(request));

        g_assert_true(webkit_uri_scheme_request_get_web_view(request) == test->m_webView);

        const char* scheme = webkit_uri_scheme_request_get_scheme(request);
        g_assert_nonnull(scheme);
        g_assert_true(test->m_handlersMap.contains(String::fromUTF8(scheme)));

        const URISchemeHandler& handler = test->m_handlersMap.get(String::fromUTF8(scheme));

        GRefPtr<GInputStream> inputStream = adoptGRef(g_memory_input_stream_new());
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(inputStream.get()));

        const gchar* requestPath = webkit_uri_scheme_request_get_path(request);

        if (!g_strcmp0(scheme, "error")) {
            if (!g_strcmp0(requestPath, "before-response")) {
                GUniquePtr<GError> error(g_error_new_literal(g_quark_from_string(errorDomain), errorCode, beforeReceiveResponseErrorMessage));
                // We call finish() and then finish_error() to make sure that not even
                // the didReceiveResponse message is processed at the time of failing.
                webkit_uri_scheme_request_finish(request, G_INPUT_STREAM(inputStream.get()), handler.replyLength, handler.mimeType.data());
                webkit_uri_scheme_request_finish_error(request, error.get());
            } else if (!g_strcmp0(requestPath, "after-first-chunk")) {
                g_memory_input_stream_add_data(G_MEMORY_INPUT_STREAM(inputStream.get()), handler.reply.data(), handler.reply.length(), 0);
                webkit_uri_scheme_request_finish(request, inputStream.get(), handler.replyLength, handler.mimeType.data());
                // We need to wait until we reach the load-committed state before calling webkit_uri_scheme_request_finish_error(),
                // so we rely on the test using finishOnCommittedAndWaitUntilLoadFinished() to actually call it from loadCommitted().
            } else {
                GUniquePtr<GError> error(g_error_new_literal(g_quark_from_string(errorDomain), errorCode, genericErrorMessage));
                webkit_uri_scheme_request_finish_error(request, error.get());
            }
            return;
        }

        if (!g_strcmp0(scheme, "echo")) {
            char* replyHTML = g_strdup_printf(handler.reply.data(), requestPath);
            g_memory_input_stream_add_data(G_MEMORY_INPUT_STREAM(inputStream.get()), replyHTML, strlen(replyHTML), g_free);
        } else if (!g_strcmp0(scheme, "closed"))
            g_input_stream_close(inputStream.get(), 0, 0);
        else if (!handler.reply.isNull())
            g_memory_input_stream_add_data(G_MEMORY_INPUT_STREAM(inputStream.get()), handler.reply.data(), handler.reply.length(), 0);

        webkit_uri_scheme_request_finish(request, inputStream.get(), handler.replyLength, handler.mimeType.data());
    }

    void registerURISchemeHandler(const char* scheme, const char* reply, int replyLength, const char* mimeType)
    {
        m_handlersMap.set(String::fromUTF8(scheme), URISchemeHandler(reply, replyLength, mimeType));
        webkit_web_context_register_uri_scheme(m_webContext.get(), scheme, uriSchemeRequestCallback, this, 0);
    }

    GRefPtr<WebKitURISchemeRequest> m_uriSchemeRequest;
    HashMap<String, URISchemeHandler> m_handlersMap;
};

String generateHTMLContent(unsigned contentLength)
{
    String baseString("abcdefghijklmnopqrstuvwxyz0123457890");
    unsigned baseLength = baseString.length();

    StringBuilder builder;
    builder.append("<html><body>");

    if (contentLength <= baseLength)
        builder.appendSubstring(baseString, 0, contentLength);
    else {
        unsigned currentLength = 0;
        while (currentLength < contentLength) {
            if ((currentLength + baseLength) <= contentLength)
                builder.append(baseString);
            else
                builder.appendSubstring(baseString, 0, contentLength - currentLength);

            // Account for the 12 characters of the '<html><body>' prefix.
            currentLength = builder.length() - 12;
        }
    }
    builder.append("</body></html>");

    return builder.toString();
}

static void testWebContextURIScheme(URISchemeTest* test, gconstpointer)
{
    test->registerURISchemeHandler("foo", kBarHTML, strlen(kBarHTML), "text/html");
    test->loadURI("foo:blank");
    test->waitUntilLoadFinished();
    size_t mainResourceDataSize = 0;
    const char* mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpint(mainResourceDataSize, ==, strlen(kBarHTML));
    g_assert_cmpint(strncmp(mainResourceData, kBarHTML, mainResourceDataSize), ==, 0);

    test->registerURISchemeHandler("echo", kEchoHTMLFormat, -1, "text/html");
    test->loadURI("echo:hello-world");
    test->waitUntilLoadFinished();
    GUniquePtr<char> echoHTML(g_strdup_printf(kEchoHTMLFormat, webkit_uri_scheme_request_get_path(test->m_uriSchemeRequest.get())));
    mainResourceDataSize = 0;
    mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpint(mainResourceDataSize, ==, strlen(echoHTML.get()));
    g_assert_cmpint(strncmp(mainResourceData, echoHTML.get(), mainResourceDataSize), ==, 0);

    test->loadURI("echo:with#fragment");
    test->waitUntilLoadFinished();
    g_assert_cmpstr(webkit_uri_scheme_request_get_path(test->m_uriSchemeRequest.get()), ==, "with");
    g_assert_cmpstr(webkit_uri_scheme_request_get_uri(test->m_uriSchemeRequest.get()), ==, "echo:with#fragment");
    echoHTML.reset(g_strdup_printf(kEchoHTMLFormat, webkit_uri_scheme_request_get_path(test->m_uriSchemeRequest.get())));
    mainResourceDataSize = 0;
    mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpint(mainResourceDataSize, ==, strlen(echoHTML.get()));
    g_assert_cmpint(strncmp(mainResourceData, echoHTML.get(), mainResourceDataSize), ==, 0);

    test->registerURISchemeHandler("nomime", kBarHTML, -1, nullptr);
    test->m_loadEvents.clear();
    test->loadURI("nomime:foo-bar");
    test->waitUntilLoadFinished();
    g_assert_true(test->m_loadEvents.contains(LoadTrackingTest::ProvisionalLoadFailed));

    static const char* charsetHTML = "<html><body><p id='emoji'>🙂</p></body></html>";
    test->registerURISchemeHandler("charset", charsetHTML, -1, "text/html; charset=\"UTF-8\"");
    test->loadURI("charset:test");
    test->waitUntilLoadFinished();
    mainResourceDataSize = 0;
    mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpint(mainResourceDataSize, ==, strlen(charsetHTML));
    g_assert_cmpint(strncmp(mainResourceData, charsetHTML, mainResourceDataSize), ==, 0);
    GUniqueOutPtr<GError> error;
    auto* javascriptResult = test->runJavaScriptAndWaitUntilFinished("document.getElementById('emoji').innerText", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    GUniquePtr<char> emoji(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(emoji.get(), ==, "🙂");

    test->registerURISchemeHandler("empty", nullptr, 0, "text/html");
    test->m_loadEvents.clear();
    test->loadURI("empty:nothing");
    test->waitUntilLoadFinished();
    g_assert_false(test->m_loadEvents.contains(LoadTrackingTest::ProvisionalLoadFailed));
    g_assert_false(test->m_loadEvents.contains(LoadTrackingTest::LoadFailed));

    // Anything over 8192 bytes will get multiple calls to g_input_stream_read_async in
    // WebKitURISchemeRequest when reading data, but we still need way more than that to
    // ensure that we reach the load-committed state before failing, so we use an 8MB HTML.
    String longHTMLContent = generateHTMLContent(8 * 1024 * 1024);
    test->registerURISchemeHandler("error", longHTMLContent.utf8().data(), -1, "text/html");
    test->m_loadEvents.clear();
    test->loadURI("error:error");
    test->waitUntilLoadFinished();
    g_assert_true(test->m_loadEvents.contains(LoadTrackingTest::ProvisionalLoadFailed));
    g_assert_true(test->m_loadFailed);
    g_assert_error(test->m_error.get(), g_quark_from_string(errorDomain), errorCode);
    g_assert_cmpstr(test->m_error->message, ==, genericErrorMessage);

    test->m_loadEvents.clear();
    test->loadURI("error:before-response");
    test->waitUntilLoadFinished();
    g_assert_true(test->m_loadEvents.contains(LoadTrackingTest::ProvisionalLoadFailed));
    g_assert_true(test->m_loadFailed);
    g_assert_error(test->m_error.get(), g_quark_from_string(errorDomain), errorCode);
    g_assert_cmpstr(test->m_error->message, ==, beforeReceiveResponseErrorMessage);

    test->registerURISchemeHandler("closed", nullptr, 0, nullptr);
    test->m_loadEvents.clear();
    test->loadURI("closed:input-stream");
    test->waitUntilLoadFinished();
    g_assert_true(test->m_loadEvents.contains(LoadTrackingTest::ProvisionalLoadFailed));
    g_assert_true(test->m_loadFailed);
    g_assert_error(test->m_error.get(), G_IO_ERROR, G_IO_ERROR_CLOSED);
}

#if PLATFORM(GTK)
static void testWebContextSpellChecker(Test* test, gconstpointer)
{
    WebKitWebContext* webContext = test->m_webContext.get();

    // Check what happens if no spell checking language has been set.
    const gchar* const* currentLanguage = webkit_web_context_get_spell_checking_languages(webContext);
    g_assert_null(currentLanguage);

    // Set the language to a specific one.
    GRefPtr<GPtrArray> languages = adoptGRef(g_ptr_array_new());
    g_ptr_array_add(languages.get(), const_cast<gpointer>(static_cast<const void*>("en_US")));
    g_ptr_array_add(languages.get(), 0);
    webkit_web_context_set_spell_checking_languages(webContext, reinterpret_cast<const char* const*>(languages->pdata));
    currentLanguage = webkit_web_context_get_spell_checking_languages(webContext);
    g_assert_cmpuint(g_strv_length(const_cast<char**>(currentLanguage)), ==, 1);
    g_assert_cmpstr(currentLanguage[0], ==, "en_US");

    // Set the language string to list of valid languages.
    g_ptr_array_remove_index_fast(languages.get(), languages->len - 1);
    g_ptr_array_add(languages.get(), const_cast<gpointer>(static_cast<const void*>("en_GB")));
    g_ptr_array_add(languages.get(), 0);
    webkit_web_context_set_spell_checking_languages(webContext, reinterpret_cast<const char* const*>(languages->pdata));
    currentLanguage = webkit_web_context_get_spell_checking_languages(webContext);
    g_assert_cmpuint(g_strv_length(const_cast<char**>(currentLanguage)), ==, 2);
    g_assert_cmpstr(currentLanguage[0], ==, "en_US");
    g_assert_cmpstr(currentLanguage[1], ==, "en_GB");

    // Try passing a wrong language along with good ones.
    g_ptr_array_remove_index_fast(languages.get(), languages->len - 1);
    g_ptr_array_add(languages.get(), const_cast<gpointer>(static_cast<const void*>("bd_WR")));
    g_ptr_array_add(languages.get(), 0);
    webkit_web_context_set_spell_checking_languages(webContext, reinterpret_cast<const char* const*>(languages->pdata));
    currentLanguage = webkit_web_context_get_spell_checking_languages(webContext);
    g_assert_cmpuint(g_strv_length(const_cast<char**>(currentLanguage)), ==, 2);
    g_assert_cmpstr(currentLanguage[0], ==, "en_US");
    g_assert_cmpstr(currentLanguage[1], ==, "en_GB");

    // Try passing a list with only wrong languages.
    languages = adoptGRef(g_ptr_array_new());
    g_ptr_array_add(languages.get(), const_cast<gpointer>(static_cast<const void*>("bd_WR")));
    g_ptr_array_add(languages.get(), const_cast<gpointer>(static_cast<const void*>("wr_BD")));
    g_ptr_array_add(languages.get(), 0);
    webkit_web_context_set_spell_checking_languages(webContext, reinterpret_cast<const char* const*>(languages->pdata));
    currentLanguage = webkit_web_context_get_spell_checking_languages(webContext);
    g_assert_null(currentLanguage);

    // Check disabling and re-enabling spell checking.
    webkit_web_context_set_spell_checking_enabled(webContext, FALSE);
    g_assert_false(webkit_web_context_get_spell_checking_enabled(webContext));
    webkit_web_context_set_spell_checking_enabled(webContext, TRUE);
    g_assert_true(webkit_web_context_get_spell_checking_enabled(webContext));
}
#endif // PLATFORM(GTK)

static void testWebContextLanguages(WebViewTest* test, gconstpointer)
{
    static const char* expectedDefaultLanguage = "en-US";
    test->loadURI(kServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    size_t mainResourceDataSize = 0;
    const char* mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpuint(mainResourceDataSize, ==, strlen(expectedDefaultLanguage));
    g_assert_cmpint(strncmp(mainResourceData, expectedDefaultLanguage, mainResourceDataSize), ==, 0);

    GRefPtr<GPtrArray> languages = adoptGRef(g_ptr_array_new());
    g_ptr_array_add(languages.get(), const_cast<gpointer>(static_cast<const void*>("en")));
    g_ptr_array_add(languages.get(), const_cast<gpointer>(static_cast<const void*>("ES_es")));
    g_ptr_array_add(languages.get(), const_cast<gpointer>(static_cast<const void*>("dE")));
    g_ptr_array_add(languages.get(), 0);
    webkit_web_context_set_preferred_languages(test->m_webContext.get(), reinterpret_cast<const char* const*>(languages->pdata));

    static const char* expectedLanguages = "en,ES-es;q=0.90,dE;q=0.80";
    test->loadURI(kServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    mainResourceDataSize = 0;
    mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpuint(mainResourceDataSize, ==, strlen(expectedLanguages));
    g_assert_cmpint(strncmp(mainResourceData, expectedLanguages, mainResourceDataSize), ==, 0);

    // When using the C locale, en-US should be used as default.
    const char* cLanguage[] = { "C", nullptr };
    webkit_web_context_set_preferred_languages(test->m_webContext.get(), cLanguage);
    GUniqueOutPtr<GError> error;
    WebKitJavascriptResult* javascriptResult = test->runJavaScriptAndWaitUntilFinished("Intl.DateTimeFormat().resolvedOptions().locale", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    GUniquePtr<char> locale(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(locale.get(), ==, expectedDefaultLanguage);

    // When using the POSIX locale, en-US should be used as default.
    const char* posixLanguage[] = { "POSIX", nullptr };
    webkit_web_context_set_preferred_languages(test->m_webContext.get(), posixLanguage);
    javascriptResult = test->runJavaScriptAndWaitUntilFinished("Intl.DateTimeFormat().resolvedOptions().locale", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    locale.reset(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(locale.get(), ==, expectedDefaultLanguage);

    // An invalid locale should not be used.
    const char* invalidLanguage[] = { "A", nullptr };
    webkit_web_context_set_preferred_languages(test->m_webContext.get(), invalidLanguage);
    javascriptResult = test->runJavaScriptAndWaitUntilFinished("Intl.DateTimeFormat().resolvedOptions().locale", &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    locale.reset(WebViewTest::javascriptResultToCString(javascriptResult));
    g_assert_cmpstr(locale.get(), !=, "A");
}

#if USE(SOUP2)
static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext* context, gpointer)
#else
static void serverCallback(SoupServer* server, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
#endif
{
    if (soup_server_message_get_method(message) != SOUP_METHOD_GET) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED, nullptr);
        return;
    }

    auto* responseBody = soup_server_message_get_response_body(message);

    if (g_str_equal(path, "/")) {
        const char* acceptLanguage = soup_message_headers_get_one(soup_server_message_get_request_headers(message), "Accept-Language");
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
        soup_message_body_append(responseBody, SOUP_MEMORY_COPY, acceptLanguage, strlen(acceptLanguage));
        soup_message_body_complete(responseBody);
    } else if (g_str_equal(path, "/empty")) {
        const char* emptyHTML = "<html><body></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, emptyHTML, strlen(emptyHTML));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else if (g_str_equal(path, "/echoPort")) {
#if USE(SOUP2)
        char* port = g_strdup_printf("%u", g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(soup_client_context_get_local_address(context))));
#else
        char* port = g_strdup_printf("%u", g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(soup_server_message_get_local_address(message))));
#endif
        soup_message_body_append(responseBody, SOUP_MEMORY_TAKE, port, strlen(port));
        soup_message_body_complete(responseBody);
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    } else
        soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
}

class SecurityPolicyTest: public Test {
public:
    MAKE_GLIB_TEST_FIXTURE(SecurityPolicyTest);

    enum SecurityPolicy {
        Local = 1 << 1,
        NoAccess = 1 << 2,
        DisplayIsolated = 1 << 3,
        Secure = 1 << 4,
        CORSEnabled = 1 << 5,
        EmptyDocument = 1 << 6
    };

    SecurityPolicyTest()
        : m_manager(webkit_web_context_get_security_manager(m_webContext.get()))
    {
    }

    void verifyThatSchemeMatchesPolicy(const char* scheme, unsigned policy)
    {
        if (policy & Local)
            g_assert_true(webkit_security_manager_uri_scheme_is_local(m_manager, scheme));
        else
            g_assert_false(webkit_security_manager_uri_scheme_is_local(m_manager, scheme));
        if (policy & NoAccess)
            g_assert_true(webkit_security_manager_uri_scheme_is_no_access(m_manager, scheme));
        else
            g_assert_false(webkit_security_manager_uri_scheme_is_no_access(m_manager, scheme));
        if (policy & DisplayIsolated)
            g_assert_true(webkit_security_manager_uri_scheme_is_display_isolated(m_manager, scheme));
        else
            g_assert_false(webkit_security_manager_uri_scheme_is_display_isolated(m_manager, scheme));
        if (policy & Secure)
            g_assert_true(webkit_security_manager_uri_scheme_is_secure(m_manager, scheme));
        else
            g_assert_false(webkit_security_manager_uri_scheme_is_secure(m_manager, scheme));
        if (policy & CORSEnabled)
            g_assert_true(webkit_security_manager_uri_scheme_is_cors_enabled(m_manager, scheme));
        else
            g_assert_false(webkit_security_manager_uri_scheme_is_cors_enabled(m_manager, scheme));
        if (policy & EmptyDocument)
            g_assert_true(webkit_security_manager_uri_scheme_is_empty_document(m_manager, scheme));
        else
            g_assert_false(webkit_security_manager_uri_scheme_is_empty_document(m_manager, scheme));
    }

    WebKitSecurityManager* m_manager;
};

static void testWebContextSecurityPolicy(SecurityPolicyTest* test, gconstpointer)
{
    // VerifyThatSchemeMatchesPolicy default policy for well known schemes.
    test->verifyThatSchemeMatchesPolicy("http", SecurityPolicyTest::CORSEnabled);
    test->verifyThatSchemeMatchesPolicy("https", SecurityPolicyTest::CORSEnabled | SecurityPolicyTest::Secure);
    test->verifyThatSchemeMatchesPolicy("file", SecurityPolicyTest::Local);
    test->verifyThatSchemeMatchesPolicy("data", SecurityPolicyTest::NoAccess | SecurityPolicyTest::Secure);
    test->verifyThatSchemeMatchesPolicy("about", SecurityPolicyTest::NoAccess | SecurityPolicyTest::Secure | SecurityPolicyTest::EmptyDocument);

    // Custom scheme.
    test->verifyThatSchemeMatchesPolicy("foo", 0);

    webkit_security_manager_register_uri_scheme_as_local(test->m_manager, "foo");
    test->verifyThatSchemeMatchesPolicy("foo", SecurityPolicyTest::Local);
    webkit_security_manager_register_uri_scheme_as_no_access(test->m_manager, "foo");
    test->verifyThatSchemeMatchesPolicy("foo", SecurityPolicyTest::Local | SecurityPolicyTest::NoAccess);
    webkit_security_manager_register_uri_scheme_as_display_isolated(test->m_manager, "foo");
    test->verifyThatSchemeMatchesPolicy("foo", SecurityPolicyTest::Local | SecurityPolicyTest::NoAccess | SecurityPolicyTest::DisplayIsolated);
    webkit_security_manager_register_uri_scheme_as_secure(test->m_manager, "foo");
    test->verifyThatSchemeMatchesPolicy("foo", SecurityPolicyTest::Local | SecurityPolicyTest::NoAccess | SecurityPolicyTest::DisplayIsolated | SecurityPolicyTest::Secure);
    webkit_security_manager_register_uri_scheme_as_cors_enabled(test->m_manager, "foo");
    test->verifyThatSchemeMatchesPolicy("foo", SecurityPolicyTest::Local | SecurityPolicyTest::NoAccess | SecurityPolicyTest::DisplayIsolated | SecurityPolicyTest::Secure
        | SecurityPolicyTest::CORSEnabled);
    webkit_security_manager_register_uri_scheme_as_empty_document(test->m_manager, "foo");
    test->verifyThatSchemeMatchesPolicy("foo", SecurityPolicyTest::Local | SecurityPolicyTest::NoAccess | SecurityPolicyTest::DisplayIsolated | SecurityPolicyTest::Secure
        | SecurityPolicyTest::CORSEnabled | SecurityPolicyTest::EmptyDocument);
}

static void consoleMessageReceivedCallback(WebKitUserContentManager*, WebKitJavascriptResult* message, Vector<WebKitJavascriptResult*>* result)
{
    g_assert_nonnull(message);
    g_assert_nonnull(result);
    result->append(webkit_javascript_result_ref(message));
}

static void testWebContextSecurityFileXHR(WebViewTest* test, gconstpointer)
{
    GUniquePtr<char> fileURL(g_strdup_printf("file://%s/simple.html", Test::getResourcesDir(Test::WebKit2Resources).data()));
    test->loadURI(fileURL.get());
    test->waitUntilLoadFinished();

    GUniquePtr<char> jsonURL(g_strdup_printf("file://%s/simple.json", Test::getResourcesDir().data()));
    GUniquePtr<char> xhr(g_strdup_printf("var xhr = new XMLHttpRequest; xhr.open(\"GET\", \"%s\"); xhr.send();", jsonURL.get()));

    Vector<WebKitJavascriptResult*> consoleMessages;
    webkit_user_content_manager_register_script_message_handler(test->m_userContentManager.get(), "console");
    g_signal_connect(test->m_userContentManager.get(), "script-message-received::console", G_CALLBACK(consoleMessageReceivedCallback), &consoleMessages);

    // By default file access is not allowed, this will show a console message with a cross-origin error.
    GUniqueOutPtr<GError> error;
    WebKitJavascriptResult* javascriptResult = test->runJavaScriptAndWaitUntilFinished(xhr.get(), &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    g_assert_cmpuint(consoleMessages.size(), ==, 2);
    Vector<GUniquePtr<char>, 2> expectedMessages;
    expectedMessages.append(g_strdup("Cross origin requests are only supported for HTTP."));
    expectedMessages.append(g_strdup_printf("XMLHttpRequest cannot load %s due to access control checks.", jsonURL.get()));
    unsigned i = 0;
    for (auto* consoleMessage : consoleMessages) {
        g_assert_nonnull(consoleMessage);
        GUniquePtr<char> messageString(WebViewTest::javascriptResultToCString(consoleMessage));
        GRefPtr<GVariant> variant = g_variant_parse(G_VARIANT_TYPE("(uusus)"), messageString.get(), nullptr, nullptr, nullptr);
        g_assert_nonnull(variant.get());
        unsigned level;
        const char* messageText;
        g_variant_get(variant.get(), "(uu&su&s)", nullptr, &level, &messageText, nullptr, nullptr);
        g_assert_cmpuint(level, ==, 3); // Console error message.
        g_assert_cmpstr(messageText, ==, expectedMessages[i++].get());
        webkit_javascript_result_unref(consoleMessage);
    }
    consoleMessages.clear();

    // Allow file access from file URLs.
    webkit_settings_set_allow_file_access_from_file_urls(webkit_web_view_get_settings(test->m_webView), TRUE);
    test->loadURI(fileURL.get());
    test->waitUntilLoadFinished();
    javascriptResult = test->runJavaScriptAndWaitUntilFinished(xhr.get(), &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());

    // It isn't still possible to load file from an HTTP URL.
    test->loadURI(kServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    javascriptResult = test->runJavaScriptAndWaitUntilFinished(xhr.get(), &error.outPtr());
    g_assert_nonnull(javascriptResult);
    g_assert_no_error(error.get());
    i = 0;
    for (auto* consoleMessage : consoleMessages) {
        g_assert_nonnull(consoleMessage);
        GUniquePtr<char> messageString(WebViewTest::javascriptResultToCString(consoleMessage));
        GRefPtr<GVariant> variant = g_variant_parse(G_VARIANT_TYPE("(uusus)"), messageString.get(), nullptr, nullptr, nullptr);
        g_assert_nonnull(variant.get());
        unsigned level;
        const char* messageText;
        g_variant_get(variant.get(), "(uu&su&s)", nullptr, &level, &messageText, nullptr, nullptr);
        g_assert_cmpuint(level, ==, 3); // Console error message.
        g_assert_cmpstr(messageText, ==, expectedMessages[i++].get());
        webkit_javascript_result_unref(consoleMessage);
    }
    consoleMessages.clear();

    g_signal_handlers_disconnect_matched(test->m_userContentManager.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, &consoleMessages);
    webkit_user_content_manager_unregister_script_message_handler(test->m_userContentManager.get(), "console");

    webkit_settings_set_allow_file_access_from_file_urls(webkit_web_view_get_settings(test->m_webView), FALSE);
}

class ProxyTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ProxyTest);

#if SOUP_CHECK_VERSION(2, 61, 90)
    enum class WebSocketServerType {
        Unknown,
        NoProxy,
        Proxy
    };

#if USE(SOUP2)
    static void webSocketProxyServerCallback(SoupServer*, SoupWebsocketConnection*, const char* path, SoupClientContext*, gpointer userData)
#else
    static void webSocketProxyServerCallback(SoupServer*, SoupServerMessage*, const char* path, SoupWebsocketConnection*, gpointer userData)
#endif
    {
        static_cast<ProxyTest*>(userData)->webSocketConnected(ProxyTest::WebSocketServerType::Proxy);
    }
#endif

    ProxyTest()
    {
        // This "proxy server" is actually just a different instance of the main
        // test server (kServer), listening on a different port. Requests
        // will not actually be proxied to kServer because proxyServer is not
        // actually a proxy server. We're testing whether the proxy settings
        // work, not whether we can write a soup proxy server.
        m_proxyServer.run(serverCallback);
        g_assert_false(m_proxyServer.baseURL().isNull());
#if SOUP_CHECK_VERSION(2, 61, 90)
        m_proxyServer.addWebSocketHandler(webSocketProxyServerCallback, this);
        g_assert_false(m_proxyServer.baseWebSocketURL().isNull());
#endif
    }

    CString loadURIAndGetMainResourceData(const char* uri)
    {
        loadURI(uri);
        waitUntilLoadFinished();
        size_t dataSize = 0;
        const char* data = mainResourceData(dataSize);
        return CString(data, dataSize);
    }

    GUniquePtr<char> proxyServerPortAsString()
    {
        GUniquePtr<char> port(g_strdup_printf("%u", m_proxyServer.port()));
        return port;
    }

#if SOUP_CHECK_VERSION(2, 61, 90)
    void webSocketConnected(WebSocketServerType serverType)
    {
        m_webSocketRequestReceived = serverType;
        quitMainLoop();
    }

    WebSocketServerType createWebSocketAndWaitUntilConnected()
    {
        m_webSocketRequestReceived = WebSocketServerType::Unknown;
        GUniquePtr<char> createWebSocket(g_strdup_printf("var ws = new WebSocket('%s');", kServer->getWebSocketURIForPath("/foo").data()));
        webkit_web_view_run_javascript(m_webView, createWebSocket.get(), nullptr, nullptr, nullptr);
        g_main_loop_run(m_mainLoop);
        return m_webSocketRequestReceived;
    }
#endif

    WebKitTestServer m_proxyServer;

#if SOUP_CHECK_VERSION(2, 61, 90)
    WebSocketServerType m_webSocketRequestReceived { WebSocketServerType::Unknown };
#endif
};

#if SOUP_CHECK_VERSION(2, 61, 90)
#if USE(SOUP2)
static void webSocketServerCallback(SoupServer*, SoupWebsocketConnection*, const char*, SoupClientContext*, gpointer userData)
#else
static void webSocketServerCallback(SoupServer*, SoupServerMessage*, const char*, SoupWebsocketConnection*, gpointer userData)
#endif
{
    static_cast<ProxyTest*>(userData)->webSocketConnected(ProxyTest::WebSocketServerType::NoProxy);
}
#endif

static void ephemeralViewloadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent, WebViewTest* test)
{
    if (loadEvent != WEBKIT_LOAD_FINISHED)
        return;
    g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(ephemeralViewloadChanged), test);
    test->quitMainLoop();
}

static void testWebContextProxySettings(ProxyTest* test, gconstpointer)
{
    // Proxy URI is unset by default. Requests to kServer should be received by kServer.
    GUniquePtr<char> serverPortAsString(g_strdup_printf("%u", kServer->port()));
    auto mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, serverPortAsString.get());

#if SOUP_CHECK_VERSION(2, 61, 90)
    // WebSocket requests should also be received by kServer.
    kServer->addWebSocketHandler(webSocketServerCallback, test);
    auto serverType = test->createWebSocketAndWaitUntilConnected();
    g_assert_true(serverType == ProxyTest::WebSocketServerType::NoProxy);
#endif

    // Set default proxy URI to point to proxyServer. Requests to kServer should be received by proxyServer instead.
    WebKitNetworkProxySettings* settings = webkit_network_proxy_settings_new(test->m_proxyServer.baseURL().string().utf8().data(), nullptr);
    auto* dataManager = webkit_web_context_get_website_data_manager(test->m_webContext.get());
    webkit_website_data_manager_set_network_proxy_settings(dataManager, WEBKIT_NETWORK_PROXY_MODE_CUSTOM, settings);
    GUniquePtr<char> proxyServerPortAsString = test->proxyServerPortAsString();
    mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, proxyServerPortAsString.get());
    webkit_network_proxy_settings_free(settings);

#if SOUP_CHECK_VERSION(2, 61, 90)
    // WebSocket requests should also be received by proxyServer.
    serverType = test->createWebSocketAndWaitUntilConnected();
    g_assert_true(serverType == ProxyTest::WebSocketServerType::Proxy);
#endif

    // Proxy settings also affect ephemeral web views.
    auto webView = Test::adoptView(g_object_new(WEBKIT_TYPE_WEB_VIEW,
#if PLATFORM(WPE)
        "backend", Test::createWebViewBackend(),
#endif
        "web-context", test->m_webContext.get(),
        "is-ephemeral", TRUE,
        nullptr));
    g_assert_true(webkit_web_view_is_ephemeral(webView.get()));
    g_assert_false(webkit_web_context_is_ephemeral(webkit_web_view_get_context(webView.get())));

    g_signal_connect(webView.get(), "load-changed", G_CALLBACK(ephemeralViewloadChanged), test);
    webkit_web_view_load_uri(webView.get(), kServer->getURIForPath("/echoPort").data());
    g_main_loop_run(test->m_mainLoop);
    WebKitWebResource* resource = webkit_web_view_get_main_resource(webView.get());
    g_assert_true(WEBKIT_IS_WEB_RESOURCE(resource));
    webkit_web_resource_get_data(resource, nullptr, [](GObject* object, GAsyncResult* result, gpointer userData) {
        size_t dataSize;
        GUniquePtr<char> data(reinterpret_cast<char*>(webkit_web_resource_get_data_finish(WEBKIT_WEB_RESOURCE(object), result, &dataSize, nullptr)));
        g_assert_nonnull(data);
        auto* test = static_cast<ProxyTest*>(userData);
        GUniquePtr<char> proxyServerPortAsString = test->proxyServerPortAsString();
        ASSERT_CMP_CSTRING(CString(data.get(), dataSize), ==, proxyServerPortAsString.get());
        test->quitMainLoop();
        }, test);
    g_main_loop_run(test->m_mainLoop);

    // Remove the proxy. Requests to kServer should be received by kServer again.
    webkit_website_data_manager_set_network_proxy_settings(dataManager, WEBKIT_NETWORK_PROXY_MODE_NO_PROXY, nullptr);
    mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, serverPortAsString.get());

    // Use a default proxy uri, but ignoring requests to localhost.
    static const char* ignoreHosts[] = { "localhost", nullptr };
    settings = webkit_network_proxy_settings_new(test->m_proxyServer.baseURL().string().utf8().data(), ignoreHosts);
    webkit_website_data_manager_set_network_proxy_settings(dataManager, WEBKIT_NETWORK_PROXY_MODE_CUSTOM, settings);
    mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, proxyServerPortAsString.get());
    GUniquePtr<char> localhostEchoPortURI(g_strdup_printf("http://localhost:%s/echoPort", serverPortAsString.get()));
    mainResourceData = test->loadURIAndGetMainResourceData(localhostEchoPortURI.get());
    ASSERT_CMP_CSTRING(mainResourceData, ==, serverPortAsString.get());
    webkit_network_proxy_settings_free(settings);

    // Remove the proxy again to ensure next test is not using any previous values.
    webkit_website_data_manager_set_network_proxy_settings(dataManager, WEBKIT_NETWORK_PROXY_MODE_NO_PROXY, nullptr);
    mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, serverPortAsString.get());

    // Use scheme specific proxy instead of the default.
    settings = webkit_network_proxy_settings_new(nullptr, nullptr);
    webkit_network_proxy_settings_add_proxy_for_scheme(settings, "http", test->m_proxyServer.baseURL().string().utf8().data());
    webkit_website_data_manager_set_network_proxy_settings(dataManager, WEBKIT_NETWORK_PROXY_MODE_CUSTOM, settings);
    mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, proxyServerPortAsString.get());
    webkit_network_proxy_settings_free(settings);

    // Reset to use the default resolver.
    webkit_website_data_manager_set_network_proxy_settings(dataManager, WEBKIT_NETWORK_PROXY_MODE_DEFAULT, nullptr);
    mainResourceData = test->loadURIAndGetMainResourceData(kServer->getURIForPath("/echoPort").data());
    ASSERT_CMP_CSTRING(mainResourceData, ==, serverPortAsString.get());

#if SOUP_CHECK_VERSION(2, 61, 90)
    kServer->removeWebSocketHandler();
#endif
}

class MemoryPressureTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE_WITH_SETUP_TEARDOWN(MemoryPressureTest, setup, teardown);

    static void setup()
    {
        Test::s_memoryPressureSettings = webkit_memory_pressure_settings_new();
        webkit_memory_pressure_settings_set_memory_limit(Test::s_memoryPressureSettings, 20);
        webkit_memory_pressure_settings_set_poll_interval(Test::s_memoryPressureSettings, 0.5);
        webkit_memory_pressure_settings_set_kill_threshold(Test::s_memoryPressureSettings, 1);
        webkit_memory_pressure_settings_set_strict_threshold(Test::s_memoryPressureSettings, 0.75);
        webkit_memory_pressure_settings_set_conservative_threshold(Test::s_memoryPressureSettings, 0.5);
    }

    static void teardown()
    {
        webkit_memory_pressure_settings_free(Test::s_memoryPressureSettings);
        Test::s_memoryPressureSettings = nullptr;
    }

    static void webProcessTerminatedCallback(WebKitWebView* webView, WebKitWebProcessTerminationReason reason, MemoryPressureTest* test)
    {
        test->m_terminationReason = reason;
        g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(webProcessTerminatedCallback), test);
        g_main_loop_quit(test->m_mainLoop);
    }

    void waitUntilWebProcessTerminated()
    {
        g_signal_connect_after(m_webView, "web-process-terminated", G_CALLBACK(MemoryPressureTest::webProcessTerminatedCallback), this);
        g_main_loop_run(m_mainLoop);
    }

    WebKitWebProcessTerminationReason m_terminationReason { WEBKIT_WEB_PROCESS_CRASHED };
};

static void testMemoryPressureSettings(MemoryPressureTest* test, gconstpointer)
{
    // Before testing the settings that have been set to the context, use a new instance
    // of WebKitMemoryPressureSettings to test the default values, getters and setters.
    WebKitMemoryPressureSettings* settings = webkit_memory_pressure_settings_new();

    // We can't exactly know the default value for the memory limit, as it depends on
    // the hardware of the machine, so just ensure that it's something > 0 and <= 3GB.
    guint limit = webkit_memory_pressure_settings_get_memory_limit(settings);
    g_assert_cmpuint(limit, >, 0);
    g_assert_cmpuint(limit * MB, <=, 3 * GB);
    g_assert_cmpfloat(webkit_memory_pressure_settings_get_conservative_threshold(settings), ==, 0.33);
    g_assert_cmpfloat(webkit_memory_pressure_settings_get_strict_threshold(settings), ==, 0.5);
    g_assert_cmpfloat(webkit_memory_pressure_settings_get_kill_threshold(settings), ==, 0);
    g_assert_cmpfloat(webkit_memory_pressure_settings_get_poll_interval(settings), ==, 30);

    // Test setting and getting values.
    webkit_memory_pressure_settings_set_memory_limit(settings, 20);
    g_assert_cmpuint(webkit_memory_pressure_settings_get_memory_limit(settings), ==, 20);

    webkit_memory_pressure_settings_set_conservative_threshold(settings, 0.4);
    g_assert_cmpfloat(webkit_memory_pressure_settings_get_conservative_threshold(settings), ==, 0.4);

    webkit_memory_pressure_settings_set_strict_threshold(settings, 0.6);
    g_assert_cmpfloat(webkit_memory_pressure_settings_get_strict_threshold(settings), ==, 0.6);

    webkit_memory_pressure_settings_set_kill_threshold(settings, 1.1);
    g_assert_cmpfloat(webkit_memory_pressure_settings_get_kill_threshold(settings), ==, 1.1);

    webkit_memory_pressure_settings_set_poll_interval(settings, 5);
    g_assert_cmpfloat(webkit_memory_pressure_settings_get_poll_interval(settings), ==, 5);

    webkit_memory_pressure_settings_free(settings);

    // An empty view uses around 7MB of memory. We're setting a maximum of 20MB here, polling every 0.5 seconds,
    // and the kill fraction is 1, so the web process will be killed when it exceeds 20MB.
    // The test html will allocate a canvas of 3000x3000, which requires around 36MB of memory, so once it gets
    // created, the MemoryPressureHandler will detect that the memory usage is too high and kill the web process.
    // This triggers the web-process-terminated signal in the view with WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT
    // as the termination reason.

    static const char* html =
        "<html>"
        "  <body>"
        "    <script>"
        "      setTimeout(function() {"
        "        var canvas = document.getElementById('canvas');"
        "        canvas.width = 3000;"
        "        canvas.height = 3000;"
        "        var context = canvas.getContext('2d');"
        "        context.fillStyle = 'green';"
        "        context.fillRect(0, 0, 3000, 3000);"
        "      }, 0);"
        "    </script>"
        "    <canvas id='canvas'></canvas>"
        "  </body>"
        "</html>";

    test->m_expectedWebProcessCrash = true;
    test->loadHtml(html, nullptr);
    test->waitUntilWebProcessTerminated();
    g_assert_cmpuint(test->m_terminationReason, ==, WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT);
}

void beforeAll()
{
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

    Test::add("WebKitWebContext", "default-context", testWebContextDefault);
    Test::add("WebKitWebContext", "ephemeral", testWebContextEphemeral);
    URISchemeTest::add("WebKitWebContext", "uri-scheme", testWebContextURIScheme);
    // FIXME: implement spellchecker in WPE.
#if PLATFORM(GTK)
    Test::add("WebKitWebContext", "spell-checker", testWebContextSpellChecker);
#endif
    WebViewTest::add("WebKitWebContext", "languages", testWebContextLanguages);
    SecurityPolicyTest::add("WebKitSecurityManager", "security-policy", testWebContextSecurityPolicy);
    WebViewTest::add("WebKitSecurityManager", "file-xhr", testWebContextSecurityFileXHR);
    ProxyTest::add("WebKitWebContext", "proxy", testWebContextProxySettings);
    MemoryPressureTest::add("WebKitWebContext", "memory-pressure", testMemoryPressureSettings);
}

void afterAll()
{
    delete kServer;
}
