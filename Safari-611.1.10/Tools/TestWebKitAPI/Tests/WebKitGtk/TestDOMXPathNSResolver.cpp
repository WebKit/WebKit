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

static void testWebKitDOMXPathNSResolverNative(WebViewTest* test, gconstpointer)
{
    static const char* nativeXML = "<root xmlns:foo='http://www.example.org'><foo:child>SUCCESS</foo:child></root>";
    g_assert_true(test->runWebProcessTest("WebKitDOMXPathNSResolver", "native", nativeXML, "text/xml"));
}

static void testWebKitDOMXPathNSResolverCustom(WebViewTest* test, gconstpointer)
{
    static const char* customXML = "<root xmlns='http://www.example.com'><child>SUCCESS</child></root>";
    g_assert_true(test->runWebProcessTest("WebKitDOMXPathNSResolver", "custom", customXML, "text/xml"));
}

void beforeAll()
{
    WebViewTest::add("WebKitDOMXPathNSResolver", "native", testWebKitDOMXPathNSResolverNative);
    WebViewTest::add("WebKitDOMXPathNSResolver", "custom", testWebKitDOMXPathNSResolverCustom);
}

void afterAll()
{
}
