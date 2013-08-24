/*
 * Copyright (C) 2013 Igalia S.L.
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

#include "WebProcessTestRunner.h"
#include "WebViewTest.h"
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

static WebProcessTestRunner* testRunner;

static void webkitFrameTestRun(WebViewTest* test, const char* testName)
{
    static const char* testHTML = "<html><body></body></html>";
    test->loadHtml(testHTML, 0);
    test->waitUntilLoadFinished();

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", "pageID", g_variant_new_uint64(webkit_web_view_get_page_id(test->m_webView)));
    g_assert(testRunner->runTest("WebKitFrame", testName, g_variant_builder_end(&builder)));
}

static void testWebKitFrameMainFrame(WebViewTest* test, gconstpointer)
{
    webkitFrameTestRun(test, "main-frame");
}

static void testWebKitFrameURI(WebViewTest* test, gconstpointer)
{
    webkitFrameTestRun(test, "uri");
}

static void testWebKitFrameJavaScriptContext(WebViewTest* test, gconstpointer)
{
    webkitFrameTestRun(test, "javascript-context");
}

void beforeAll()
{
    testRunner = new WebProcessTestRunner();
    webkit_web_context_set_web_extensions_directory(webkit_web_context_get_default(), WEBKIT_TEST_WEB_EXTENSIONS_DIR);

    WebViewTest::add("WebKitFrame", "main-frame", testWebKitFrameMainFrame);
    WebViewTest::add("WebKitFrame", "uri", testWebKitFrameURI);
    WebViewTest::add("WebKitFrame", "javascript-context", testWebKitFrameJavaScriptContext);
}

void afterAll()
{
    delete testRunner;
}
