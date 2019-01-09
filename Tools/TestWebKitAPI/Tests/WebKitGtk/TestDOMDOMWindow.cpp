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

#define HTML_DOCUMENT "<html><head><title></title></head><style type='text/css'>#test { font-size: 16px; }</style><body><p id='test'>test</p></body></html>"

typedef struct {
    gboolean loaded;
    gboolean clicked;
    WebProcessTestRunner* testRunner;
    WebViewTest* test;
} DomDomWindowTestStatus;

static DomDomWindowTestStatus status;

static void signalsNotifyCallback(const gchar *key, const gchar *value, gconstpointer)
{
    if (g_str_equal(key, "ready")) {
        // The document was already loaded in the webprocess, and its "load"
        // signal couldn't be captured on time (was issued before the test
        // started). We load it again to force a new "load" signal in the
        // webprocess, which will be captured this time
        status.test->loadHtml(HTML_DOCUMENT, 0);
    }

    if (g_str_equal(key, "loaded")) {
        status.loaded = TRUE;
        status.test->showInWindowAndWaitUntilMapped();

        // Click in a known location where the text is
        status.test->clickMouseButton(20, 18, 1, 0);
    }

    if (g_str_equal(key, "clicked"))
        status.clicked = TRUE;

    if (g_str_equal(key, "finish")) {
        status.test = 0;
        status.testRunner->finishTest(status.loaded && status.clicked);
    }
}

static void dispatchEventNotifyCallback(const gchar *key, const gchar *value, gconstpointer)
{
    if (g_str_equal(key, "ready")) {
        // The document was already loaded in the webprocess, and its "load"
        // signal couldn't be captured on time (was issued before the test
        // started). We load it again to force a new "load" signal in the
        // webprocess, which will be captured this time
        status.test->loadHtml(HTML_DOCUMENT, 0);
    }

    if (g_str_equal(key, "loaded"))
        status.loaded = TRUE;

    if (g_str_equal(key, "clicked"))
        status.clicked = TRUE;

    if (g_str_equal(key, "finish")) {
        status.test = 0;
        status.testRunner->finishTest(status.loaded && status.clicked);
    }
}

static void testWebKitDOMDOMWindowSignals(WebViewTest* test, gconstpointer)
{
    status.loaded = FALSE;
    status.clicked = FALSE;
    status.test = test;

    status.testRunner->setNotifyCallback(G_CALLBACK(signalsNotifyCallback), 0);

    // The HTML document will we loaded later, when the test is "ready" because
    // we want to test the "load" signal

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", "pageID", g_variant_new_uint64(webkit_web_view_get_page_id(status.test->m_webView)));
    status.testRunner->runTestAndWait("WebKitDOMDOMWindow", "signals", g_variant_builder_end(&builder));
    g_assert_true(status.testRunner->getTestResult());
}

static void testWebKitDOMDOMWindowDispatchEvent(WebViewTest* test, gconstpointer)
{
    status.loaded = FALSE;
    status.clicked = FALSE;
    status.test = test;

    status.testRunner->setNotifyCallback(G_CALLBACK(dispatchEventNotifyCallback), 0);

    // The HTML document will we loaded later, when the test is "ready" because
    // we want to test the "load" signal

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", "pageID", g_variant_new_uint64(webkit_web_view_get_page_id(status.test->m_webView)));
    status.testRunner->runTestAndWait("WebKitDOMDOMWindow", "dispatch-event", g_variant_builder_end(&builder));
    g_assert_true(status.testRunner->getTestResult());
}

static void testWebKitDOMDOMWindowGetComputedStyle(WebViewTest* test, gconstpointer)
{
    status.loaded = FALSE;
    status.clicked = FALSE;
    status.test = test;

    static const char* testHTML = HTML_DOCUMENT;
    status.test->loadHtml(testHTML, 0);
    status.test->waitUntilLoadFinished();

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", "pageID", g_variant_new_uint64(webkit_web_view_get_page_id(status.test->m_webView)));
    g_assert_true(status.testRunner->runTest("WebKitDOMDOMWindow", "get-computed-style", g_variant_builder_end(&builder)));
}

void beforeAll()
{
    status.testRunner = new WebProcessTestRunner();
    webkit_web_context_set_web_extensions_directory(webkit_web_context_get_default(), WEBKIT_TEST_WEB_EXTENSIONS_DIR);

    WebViewTest::add("WebKitDOMDOMWindow", "signals", testWebKitDOMDOMWindowSignals);
    WebViewTest::add("WebKitDOMDOMWindow", "dispatch-event", testWebKitDOMDOMWindowDispatchEvent);
    WebViewTest::add("WebKitDOMDOMWindow", "get-computed-style", testWebKitDOMDOMWindowGetComputedStyle);
}

void afterAll()
{
    delete status.testRunner;
}
