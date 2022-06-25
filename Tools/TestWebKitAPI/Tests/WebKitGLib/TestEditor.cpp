/*
 * Copyright (C) 2015 Red Hat Inc.
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

static void testWebKitWebEditorSelectionChanged(WebViewTest* test, gconstpointer)
{
    static const gchar* testHTML = "<html><body>All work and no play make Jack a dull boy.</body></html>";
    g_assert_true(test->runWebProcessTest("WebKitWebEditor", "selection-changed", testHTML));
}

void beforeAll()
{
    WebViewTest::add("WebKitWebEditor", "selection-changed", testWebKitWebEditorSelectionChanged);
}

void afterAll()
{
}
