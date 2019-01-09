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

#include "WebViewTest.h"
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

static const char* testHTML = "<html id='root'><head><title>DOMNodeTreeWalker</title></head>"
    "<body><input type='button' name='push' value='push'><input type='button' name='clear' value='clear'><br></body></html>";

static void testWebKitDOMNodeFilterTreeWalker(WebViewTest* test, gconstpointer)
{
    g_assert_true(test->runWebProcessTest("WebKitDOMNodeFilter", "tree-walker", testHTML));
}

static void testWebKitDOMNodeFilterNodeIterator(WebViewTest* test, gconstpointer)
{
    g_assert_true(test->runWebProcessTest("WebKitDOMNodeFilter", "node-iterator", testHTML));
}

void beforeAll()
{
    WebViewTest::add("WebKitDOMNodeFilter", "tree-walker", testWebKitDOMNodeFilterTreeWalker);
    WebViewTest::add("WebKitDOMNodeFilter", "node-iterator", testWebKitDOMNodeFilterNodeIterator);
}

void afterAll()
{
}
