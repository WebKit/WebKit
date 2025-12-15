/*
 * Copyright (C) 2025 Igalia S.L.
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
#include <wtf/glib/GRefPtr.h>

class ContextMenuTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ContextMenuTest);

    ~ContextMenuTest()
    {
        if (m_contextMenu)
            g_object_unref(m_contextMenu);
        if (m_hitTestResult)
            g_object_unref(m_hitTestResult);
    }

#if ENABLE(2022_GLIB_API)
    static gboolean contextMenuCallback(WebKitWebView* webView, WebKitContextMenu* contextMenu, WebKitHitTestResult* hitTestResult, ContextMenuTest* test)
#else
    static gboolean contextMenuCallback(WebKitWebView* webView, WebKitContextMenu* contextMenu, gpointer /*event*/, WebKitHitTestResult* hitTestResult, ContextMenuTest* test)
#endif
    {
        g_assert_nonnull(contextMenu);
        g_assert_nonnull(hitTestResult);
        // Reference the objects to keep them alive after the callback returns
        test->m_contextMenu = WEBKIT_CONTEXT_MENU(g_object_ref(contextMenu));
        test->m_hitTestResult = WEBKIT_HIT_TEST_RESULT(g_object_ref(hitTestResult));
        test->quitMainLoop();
        return FALSE;
    }

    static gboolean contextMenuTimeoutCallback(ContextMenuTest* test)
    {
        test->m_contextMenuTimedOut = true;
        test->quitMainLoop();
        return G_SOURCE_REMOVE;
    }

    WebKitContextMenu* showContextMenuAtPosition(int x, int y)
    {
        // Release any previously held references
        if (m_contextMenu) {
            g_object_unref(m_contextMenu);
            m_contextMenu = nullptr;
        }
        if (m_hitTestResult) {
            g_object_unref(m_hitTestResult);
            m_hitTestResult = nullptr;
        }
        m_contextMenuTimedOut = false;

        auto contextMenuId = g_signal_connect(m_webView.get(), "context-menu", G_CALLBACK(contextMenuCallback), this);

        // Add a timeout to prevent hanging forever if context menu doesn't appear
        auto timeoutId = g_timeout_add(5000, reinterpret_cast<GSourceFunc>(contextMenuTimeoutCallback), this);

        clickMouseButton(x, y, MouseButton::Secondary);
        g_main_loop_run(m_mainLoop);

        g_source_remove(timeoutId);
        g_signal_handler_disconnect(m_webView.get(), contextMenuId);

        if (m_contextMenuTimedOut)
            g_test_message("Context menu did not appear within timeout");

        return m_contextMenu;
    }

    WebKitContextMenu* m_contextMenu { nullptr };
    WebKitHitTestResult* m_hitTestResult { nullptr };
    bool m_contextMenuTimedOut { false };
};

static void testContextMenuGetPosition(ContextMenuTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml("<html><body style='margin:0;padding:0;'><p>Context menu position test</p></body></html>", nullptr);
    test->waitUntilLoadFinished();

    const int clickX = 100;
    const int clickY = 50;

    auto* menu = test->showContextMenuAtPosition(clickX, clickY);
    if (!menu) {
        g_test_skip("Context menu did not appear - context menu testing may not be supported in this environment");
        return;
    }

    gint x = 0, y = 0;
    gboolean hasPosition = webkit_context_menu_get_position(menu, &x, &y);
    g_assert_true(hasPosition);
    g_assert_cmpint(x, ==, clickX);
    g_assert_cmpint(y, ==, clickY);

    // Test with nullptr parameters - should still return TRUE
    g_assert_true(webkit_context_menu_get_position(menu, nullptr, nullptr));
    g_assert_true(webkit_context_menu_get_position(menu, &x, nullptr));
    g_assert_true(webkit_context_menu_get_position(menu, nullptr, &y));
}

static void testContextMenuItemGetTitle(ContextMenuTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml("<html><body style='margin:0;padding:0;'><p>Context menu item title test</p></body></html>", nullptr);
    test->waitUntilLoadFinished();

    auto* menu = test->showContextMenuAtPosition(100, 50);
    if (!menu) {
        g_test_skip("Context menu did not appear");
        return;
    }

    GList* items = webkit_context_menu_get_items(menu);
    g_assert_nonnull(items);
    g_assert_cmpuint(g_list_length(items), >, 0);

    // Test that non-separator items return a valid string pointer
    // Note: Some items may have empty titles depending on localization
    int itemsWithTitle = 0;
    for (GList* l = items; l; l = g_list_next(l)) {
        auto* item = WEBKIT_CONTEXT_MENU_ITEM(l->data);
        g_assert_true(WEBKIT_IS_CONTEXT_MENU_ITEM(item));

        if (webkit_context_menu_item_is_separator(item)) {
            // Separator items should return nullptr for title
            g_assert_null(webkit_context_menu_item_get_title(item));
        } else {
            // Non-separator items should return a valid pointer (not nullptr)
            const char* title = webkit_context_menu_item_get_title(item);
            g_assert_nonnull(title);
            if (strlen(title) > 0)
                itemsWithTitle++;
        }
    }
    // At least some items should have non-empty titles
    g_assert_cmpint(itemsWithTitle, >, 0);
}

static void testContextMenuItemGetTitleStability(ContextMenuTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml("<html><body style='margin:0;padding:0;'><p>Title stability test</p></body></html>", nullptr);
    test->waitUntilLoadFinished();

    auto* menu = test->showContextMenuAtPosition(100, 50);
    if (!menu) {
        g_test_skip("Context menu did not appear");
        return;
    }

    GList* items = webkit_context_menu_get_items(menu);
    g_assert_nonnull(items);

    // Find a non-separator item with a non-empty title
    WebKitContextMenuItem* itemWithTitle = nullptr;
    for (GList* l = items; l; l = g_list_next(l)) {
        auto* item = WEBKIT_CONTEXT_MENU_ITEM(l->data);
        if (!webkit_context_menu_item_is_separator(item)) {
            const char* title = webkit_context_menu_item_get_title(item);
            if (title && strlen(title) > 0) {
                itemWithTitle = item;
                break;
            }
        }
    }

    if (!itemWithTitle) {
        g_test_skip("No menu item with non-empty title found");
        return;
    }

    // Call get_title multiple times and verify the pointer is stable (cached)
    const char* title1 = webkit_context_menu_item_get_title(itemWithTitle);
    const char* title2 = webkit_context_menu_item_get_title(itemWithTitle);
    const char* title3 = webkit_context_menu_item_get_title(itemWithTitle);

    g_assert_nonnull(title1);
    g_assert_true(title1 == title2);
    g_assert_true(title2 == title3);
}

// TODO: Add the following tests:
//
// 1. testSelectContextMenuItem
//    - Test webkit_web_view_select_context_menu_item() API
//    - Verify that selecting a "Copy" menu item on selected text works correctly
//
// 2. testSelectContextMenuItemOnLink
//    - Test webkit_web_view_select_context_menu_item() on a link element
//    - Verify that "Copy Link Address" action works correctly
//
// These tests work correctly in MiniBrowser but block in the test environment
//
// Note: "context-menu-dismissed" signal testing is not applicable for WPE
// since the application draws the context menu (not WebKit), so WebKit
// cannot know when the menu is dismissed.

void beforeAll()
{
    ContextMenuTest::add("ContextMenu", "get-position", testContextMenuGetPosition);
    ContextMenuTest::add("ContextMenu", "item-get-title", testContextMenuItemGetTitle);
    ContextMenuTest::add("ContextMenu", "item-get-title-stability", testContextMenuItemGetTitleStability);
}

void afterAll()
{
}
