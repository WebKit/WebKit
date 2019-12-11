/*
 * Copyright (C) 2017 Aidan Holm <aidanholm@gmail.com>
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
#include <webkit2/webkit2.h>

static const char* testHTML = "<html><head></head><body>"
    "<style>"
    "    #rect { position: fixed; top: -25px; left: -50px; width: 100px; height: 200px; }"
    "</style>"
    "<div id=rect></div></body></html>";

static void testWebKitDOMClientRectDivBoundingClientRectPosition(WebViewTest* test, gconstpointer)
{
    g_assert_true(test->runWebProcessTest("WebKitDOMClientRect", "div-bounding-client-rect-position", testHTML));
}

static void testWebKitDOMClientRectDivClientRectsPositionAndLength(WebViewTest* test, gconstpointer)
{
    g_assert_true(test->runWebProcessTest("WebKitDOMClientRect", "div-client-rects-position-and-length", testHTML));
}

void beforeAll()
{
    WebViewTest::add("WebKitDOMClientRect", "div-bounding-client-rect-position", testWebKitDOMClientRectDivBoundingClientRectPosition);
    WebViewTest::add("WebKitDOMClientRect", "div-client-rects-position-and-length", testWebKitDOMClientRectDivClientRectsPositionAndLength);
}

void afterAll()
{
}
