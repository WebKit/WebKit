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
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <wtf/HashMap.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/StringHash.h>

static WebKitTestServer* kServer;

static void testWebContextDefault(Test* test, gconstpointer)
{
    // Check there's a single instance of the default web context.
    g_assert(webkit_web_context_get_default() == webkit_web_context_get_default());
}

class PluginsTest: public Test {
public:
    MAKE_GLIB_TEST_FIXTURE(PluginsTest);

    PluginsTest()
        : m_context(webkit_web_context_get_default())
        , m_mainLoop(g_main_loop_new(0, TRUE))
        , m_plugins(0)
    {
        webkit_web_context_set_additional_plugins_directory(m_context, WEBKIT_TEST_PLUGIN_DIR);
    }

    ~PluginsTest()
    {
        g_main_loop_unref(m_mainLoop);
        g_list_free_full(m_plugins, g_object_unref);
    }

    static void getPluginsAsyncReadyCallback(GObject*, GAsyncResult* result, PluginsTest* test)
    {
        test->m_plugins = webkit_web_context_get_plugins_finish(test->m_context, result, 0);
        g_main_loop_quit(test->m_mainLoop);
    }

    GList* getPlugins()
    {
        g_list_free_full(m_plugins, g_object_unref);
        webkit_web_context_get_plugins(m_context, 0, reinterpret_cast<GAsyncReadyCallback>(getPluginsAsyncReadyCallback), this);
        g_main_loop_run(m_mainLoop);
        return m_plugins;
    }

    WebKitWebContext* m_context;
    GMainLoop* m_mainLoop;
    GList* m_plugins;
};

static void testWebContextGetPlugins(PluginsTest* test, gconstpointer)
{
    GList* plugins = test->getPlugins();
    g_assert(plugins);

    GRefPtr<WebKitPlugin> testPlugin;
    for (GList* item = plugins; item; item = g_list_next(item)) {
        WebKitPlugin* plugin = WEBKIT_PLUGIN(item->data);
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(plugin));
        if (!g_strcmp0(webkit_plugin_get_name(plugin), "WebKit Test PlugIn")) {
            testPlugin = plugin;
            break;
        }
    }
    g_assert(WEBKIT_IS_PLUGIN(testPlugin.get()));

    GOwnPtr<char> pluginPath(g_build_filename(WEBKIT_TEST_PLUGIN_DIR, "libtestnetscapeplugin.so", NULL));
    g_assert_cmpstr(webkit_plugin_get_path(testPlugin.get()), ==, pluginPath.get());
    g_assert_cmpstr(webkit_plugin_get_description(testPlugin.get()), ==, "Simple Netscape® plug-in that handles test content for WebKit");
    GList* mimeInfoList = webkit_plugin_get_mime_info_list(testPlugin.get());
    g_assert(mimeInfoList);
    g_assert_cmpuint(g_list_length(mimeInfoList), ==, 2);

    WebKitMimeInfo* mimeInfo = static_cast<WebKitMimeInfo*>(mimeInfoList->data);
    g_assert_cmpstr(webkit_mime_info_get_mime_type(mimeInfo), ==, "image/png");
    g_assert_cmpstr(webkit_mime_info_get_description(mimeInfo), ==, "png image");
    const gchar* const* extensions = webkit_mime_info_get_extensions(mimeInfo);
    g_assert(extensions);
    g_assert_cmpstr(extensions[0], ==, "png");

    mimeInfoList = g_list_next(mimeInfoList);
    mimeInfo = static_cast<WebKitMimeInfo*>(mimeInfoList->data);
    g_assert_cmpstr(webkit_mime_info_get_mime_type(mimeInfo), ==, "application/x-webkit-test-netscape");
    g_assert_cmpstr(webkit_mime_info_get_description(mimeInfo), ==, "test netscape content");
    extensions = webkit_mime_info_get_extensions(mimeInfo);
    g_assert(extensions);
    g_assert_cmpstr(extensions[0], ==, "testnetscape");
}

static const char* kBarHTML = "<html><body>Bar</body></html>";
static const char* kEchoHTMLFormat = "<html><body>%s</body></html>";

class URISchemeTest: public LoadTrackingTest {
public:
    MAKE_GLIB_TEST_FIXTURE(URISchemeTest);

    struct URISchemeHandler {
        URISchemeHandler()
            : replyLength(0)
            , replyWithPath(false)
        {
        }

        URISchemeHandler(const char* reply, int replyLength, const char* mimeType, bool replyWithPath = false)
            : reply(reply)
            , replyLength(replyLength)
            , mimeType(mimeType)
            , replyWithPath(replyWithPath)
        {
        }

        CString reply;
        int replyLength;
        CString mimeType;
        bool replyWithPath;
    };

    static void uriSchemeRequestCallback(WebKitURISchemeRequest* request, gpointer userData)
    {
        URISchemeTest* test = static_cast<URISchemeTest*>(userData);
        test->m_uriSchemeRequest = request;
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(request));

        GRefPtr<GInputStream> inputStream = adoptGRef(g_memory_input_stream_new());
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(inputStream.get()));

        String scheme(String::fromUTF8(webkit_uri_scheme_request_get_scheme(request)));
        g_assert(!scheme.isEmpty());
        g_assert(test->m_handlersMap.contains(scheme));
        const URISchemeHandler& handler = test->m_handlersMap.get(scheme);

        if (handler.replyWithPath) {
            char* replyHTML = g_strdup_printf(handler.reply.data(), webkit_uri_scheme_request_get_path(request));
            g_memory_input_stream_add_data(G_MEMORY_INPUT_STREAM(inputStream.get()), replyHTML, strlen(replyHTML), g_free);
        } else if (!handler.reply.isNull())
            g_memory_input_stream_add_data(G_MEMORY_INPUT_STREAM(inputStream.get()), handler.reply.data(), handler.reply.length(), 0);
        webkit_uri_scheme_request_finish(request, inputStream.get(), handler.replyLength, handler.mimeType.data());
    }

    void registerURISchemeHandler(const char* scheme, const char* reply, int replyLength, const char* mimeType, bool replyWithPath = false)
    {
        m_handlersMap.set(String::fromUTF8(scheme), URISchemeHandler(reply, replyLength, mimeType, replyWithPath));
        webkit_web_context_register_uri_scheme(webkit_web_context_get_default(), scheme, uriSchemeRequestCallback, this, 0);
    }

    GRefPtr<WebKitURISchemeRequest> m_uriSchemeRequest;
    HashMap<String, URISchemeHandler> m_handlersMap;
};

static void testWebContextURIScheme(URISchemeTest* test, gconstpointer)
{
    test->registerURISchemeHandler("foo", kBarHTML, strlen(kBarHTML), "text/html");
    test->loadURI("foo:blank");
    test->waitUntilLoadFinished();
    size_t mainResourceDataSize = 0;
    const char* mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpint(mainResourceDataSize, ==, strlen(kBarHTML));
    g_assert(!strncmp(mainResourceData, kBarHTML, mainResourceDataSize));

    test->registerURISchemeHandler("echo", kEchoHTMLFormat, -1, "text/html", true);
    test->loadURI("echo:hello world");
    test->waitUntilLoadFinished();
    GOwnPtr<char> echoHTML(g_strdup_printf(kEchoHTMLFormat, webkit_uri_scheme_request_get_path(test->m_uriSchemeRequest.get())));
    mainResourceDataSize = 0;
    mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpint(mainResourceDataSize, ==, strlen(echoHTML.get()));
    g_assert(!strncmp(mainResourceData, echoHTML.get(), mainResourceDataSize));

    test->registerURISchemeHandler("nomime", kBarHTML, -1, 0);
    test->m_loadEvents.clear();
    test->loadURI("nomime:foo bar");
    test->waitUntilLoadFinished();
    g_assert(test->m_loadEvents.contains(LoadTrackingTest::ProvisionalLoadFailed));

    test->registerURISchemeHandler("empty", 0, 0, "text/html");
    test->m_loadEvents.clear();
    test->loadURI("empty:nothing");
    test->waitUntilLoadFinished();
    g_assert(!test->m_loadEvents.contains(LoadTrackingTest::ProvisionalLoadFailed));
    g_assert(!test->m_loadEvents.contains(LoadTrackingTest::LoadFailed));
}

static void testWebContextSpellChecker(Test* test, gconstpointer)
{
    WebKitWebContext* webContext = webkit_web_context_get_default();

    // Check what happens if no spell checking language has been set.
    const gchar* currentLanguage = webkit_web_context_get_spell_checking_languages(webContext);
    g_assert(!currentLanguage);

    // Set the language to a specific one.
    webkit_web_context_set_spell_checking_languages(webContext, "en_US");
    currentLanguage = webkit_web_context_get_spell_checking_languages(webContext);
    g_assert_cmpstr(currentLanguage, ==, "en_US");

    // Set the language string to list of valid languages.
    webkit_web_context_set_spell_checking_languages(webContext, "en_GB,en_US");
    currentLanguage = webkit_web_context_get_spell_checking_languages(webContext);
    g_assert_cmpstr(currentLanguage, ==, "en_GB,en_US");

    // Try passing a wrong language along with good ones.
    webkit_web_context_set_spell_checking_languages(webContext, "bd_WR,en_US,en_GB");
    currentLanguage = webkit_web_context_get_spell_checking_languages(webContext);
    g_assert_cmpstr(currentLanguage, ==, "en_US,en_GB");

    // Try passing a list with only wrong languages.
    webkit_web_context_set_spell_checking_languages(webContext, "bd_WR,wr_BD");
    currentLanguage = webkit_web_context_get_spell_checking_languages(webContext);
    g_assert(!currentLanguage);

    // Check disabling and re-enabling spell checking.
    webkit_web_context_set_spell_checking_enabled(webContext, FALSE);
    g_assert(!webkit_web_context_get_spell_checking_enabled(webContext));
    webkit_web_context_set_spell_checking_enabled(webContext, TRUE);
    g_assert(webkit_web_context_get_spell_checking_enabled(webContext));
}

static void testWebContextLanguages(WebViewTest* test, gconstpointer)
{
    static const char* expectedDefaultLanguage = "en";
    test->loadURI(kServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    size_t mainResourceDataSize = 0;
    const char* mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpuint(mainResourceDataSize, ==, strlen(expectedDefaultLanguage));
    g_assert(!strncmp(mainResourceData, expectedDefaultLanguage, mainResourceDataSize));

    GList* languages = g_list_prepend(0, const_cast<gpointer>(static_cast<const void*>("dE")));
    languages = g_list_prepend(languages, const_cast<gpointer>(static_cast<const void*>("ES_es")));
    languages = g_list_prepend(languages, const_cast<gpointer>(static_cast<const void*>("en")));
    webkit_web_context_set_preferred_languages(webkit_web_context_get_default(), languages);
    g_list_free(languages);

    static const char* expectedLanguages = "en, es-es;q=0.90, de;q=0.80";
    test->loadURI(kServer->getURIForPath("/").data());
    test->waitUntilLoadFinished();
    mainResourceDataSize = 0;
    mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpuint(mainResourceDataSize, ==, strlen(expectedLanguages));
    g_assert(!strncmp(mainResourceData, expectedLanguages, mainResourceDataSize));
}

static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    soup_message_set_status(message, SOUP_STATUS_OK);
    const char* acceptLanguage = soup_message_headers_get_one(message->request_headers, "Accept-Language");
    soup_message_body_append(message->response_body, SOUP_MEMORY_COPY, acceptLanguage, strlen(acceptLanguage));
    soup_message_body_complete(message->response_body);
}

void beforeAll()
{
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

    Test::add("WebKitWebContext", "default-context", testWebContextDefault);
    PluginsTest::add("WebKitWebContext", "get-plugins", testWebContextGetPlugins);
    URISchemeTest::add("WebKitWebContext", "uri-scheme", testWebContextURIScheme);
    Test::add("WebKitWebContext", "spell-checker", testWebContextSpellChecker);
    WebViewTest::add("WebKitWebContext", "languages", testWebContextLanguages);
}

void afterAll()
{
    delete kServer;
}
