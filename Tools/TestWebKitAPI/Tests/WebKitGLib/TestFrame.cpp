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

#include "WebViewTest.h"

static void testWebKitFrameMainFrame(WebViewTest* test, gconstpointer)
{
    g_assert_true(test->runWebProcessTest("WebKitFrame", "main-frame"));
}

static void testWebKitFrameURI(WebViewTest* test, gconstpointer)
{
    g_assert_true(test->runWebProcessTest("WebKitFrame", "uri"));
}

static void testWebKitFrameJavaScriptContext(WebViewTest* test, gconstpointer)
{
    g_assert_true(test->runWebProcessTest("WebKitFrame", "javascript-context"));
}

static void testWebKitFrameJavaScriptValues(WebViewTest* test, gconstpointer)
{
    static const char* testHTML = "<html><body><p id='paragraph'>This is a test</p><img id='image' src='foo.png'></body></html>";
    g_assert_true(test->runWebProcessTest("WebKitFrame", "javascript-values", testHTML));
}

void beforeAll()
{
    WebViewTest::add("WebKitFrame", "main-frame", testWebKitFrameMainFrame);
    WebViewTest::add("WebKitFrame", "uri", testWebKitFrameURI);
    WebViewTest::add("WebKitFrame", "javascript-context", testWebKitFrameJavaScriptContext);
    WebViewTest::add("WebKitFrame", "javascript-values", testWebKitFrameJavaScriptValues);
}

void afterAll()
{
}
