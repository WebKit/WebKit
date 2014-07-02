/*
 * Copyright (C) 2014 Igalia S.L.
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

#include "WebKitTestServer.h"
#include "WebProcessTestRunner.h"
#include "WebViewTest.h"
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

static WebProcessTestRunner* testRunner;
static WebKitTestServer* kServer;

static void runTest(WebViewTest* test, const char* name)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", "pageID", g_variant_new_uint64(webkit_web_view_get_page_id(test->m_webView)));
    g_assert(testRunner->runTest("WebKitDOMXPathNSResolver", name, g_variant_builder_end(&builder)));
}

static void testWebKitDOMXPathNSResolverNative(WebViewTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/native").data());
    test->waitUntilLoadFinished();
    runTest(test, "native");
}

static void testWebKitDOMXPathNSResolverCustom(WebViewTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/custom").data());
    test->waitUntilLoadFinished();
    runTest(test, "custom");
}

static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    if (g_str_equal(path, "/native")) {
        soup_message_set_status(message, SOUP_STATUS_OK);
        soup_message_headers_append(message->response_headers, "Content-Type", "text/xml");
        static const char* nativeXML = "<root xmlns:foo='http://www.example.org'><foo:child>SUCCESS</foo:child></root>";
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, nativeXML, strlen(nativeXML));
        soup_message_body_complete(message->response_body);
    } else if (g_str_equal(path, "/custom")) {
        soup_message_set_status(message, SOUP_STATUS_OK);
        soup_message_headers_append(message->response_headers, "Content-Type", "text/xml");
        static const char* customXML = "<root xmlns='http://www.example.com'><child>SUCCESS</child></root>";
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, customXML, strlen(customXML));
        soup_message_body_complete(message->response_body);
    } else
        soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
}

void beforeAll()
{
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

    testRunner = new WebProcessTestRunner();
    webkit_web_context_set_web_extensions_directory(webkit_web_context_get_default(), WEBKIT_TEST_WEB_EXTENSIONS_DIR);

    WebViewTest::add("WebKitDOMXPathNSResolver", "native", testWebKitDOMXPathNSResolverNative);
    WebViewTest::add("WebKitDOMXPathNSResolver", "custom", testWebKitDOMXPathNSResolverCustom);
}

void afterAll()
{
    delete testRunner;
    delete kServer;
}
