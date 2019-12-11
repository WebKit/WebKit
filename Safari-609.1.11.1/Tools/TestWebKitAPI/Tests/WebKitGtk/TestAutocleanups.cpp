/*
 * Copyright (C) 2015 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC

static void testUIProcessAutocleanups(WebViewTest* test, gconstpointer)
{
    // Sanity-check a couple UI process API autocleanups that are easy to test....
    g_autoptr(WebKitWebContext) context = webkit_web_context_new();
    g_assert_true(WEBKIT_IS_WEB_CONTEXT(context));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context));

    g_autoptr(WebKitWebsiteDataManager) manager = webkit_website_data_manager_new(nullptr);
    g_assert_true(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context));

    g_autoptr(WebKitUserScript) userScript = webkit_user_script_new("",
        WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
        nullptr, nullptr);
    g_assert_nonnull(userScript);
    // Not a GObject, so just checking that this doesn't crash....
}

static void testWebProcessAutocleanups(WebViewTest* test, gconstpointer)
{
    g_assert_true(test->runWebProcessTest("Autocleanups", "web-process-autocleanups"));
}

void beforeAll()
{
    WebViewTest::add("Autocleanups", "ui-process-autocleanups", testUIProcessAutocleanups);
    WebViewTest::add("Autocleanups", "web-process-autocleanups", testWebProcessAutocleanups);
}

#else

void beforeAll()
{
}

#endif // G_DEFINE_AUTOPTR_CLEANUP_FUNC

void afterAll()
{
}
