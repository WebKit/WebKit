/*
 * Copyright (C) 2017 Igalia S.L.
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

#include <wtf/RunLoop.h>

#if PLATFORM(GTK)
using PlatformRectangle = GdkRectangle;
#elif PLATFORM(WPE)
using PlatformRectangle = WebKitRectangle;
#endif

class OptionMenuTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(OptionMenuTest);

    OptionMenuTest()
    {
        g_signal_connect(m_webView, "show-option-menu", G_CALLBACK(showOptionMenuCallback), this);
    }

    ~OptionMenuTest()
    {
        g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
        if (m_menu)
            close();
    }

    void destroyMenu()
    {
        if (!m_menu)
            return;

        g_signal_handlers_disconnect_matched(m_menu.get(), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
        m_menu = nullptr;
    }

    static gboolean showOptionMenuCallback(WebKitWebView* webView, WebKitOptionMenu* menu,
#if PLATFORM(GTK) && !USE(GTK4)
        GdkEvent* event,
#endif
        PlatformRectangle* rect, OptionMenuTest* test)
    {
        g_assert_true(test->m_webView == webView);
        g_assert_nonnull(rect);
        g_assert_true(WEBKIT_IS_OPTION_MENU(menu));
#if PLATFORM(GTK) && !USE(GTK4)
        g_assert_true(webkit_option_menu_get_event(menu) == event);
#endif
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(menu));
        test->showOptionMenu(menu, rect);
        return TRUE;
    }

    static void menuCloseCallback(WebKitOptionMenu* menu, OptionMenuTest* test)
    {
        g_assert_true(test->m_menu.get() == menu);
        test->destroyMenu();
    }

    void showOptionMenu(WebKitOptionMenu* menu, PlatformRectangle* rect)
    {
        m_rectangle = *rect;
        m_menu = menu;
        g_signal_connect(m_menu.get(), "close", G_CALLBACK(menuCloseCallback), this);
        g_main_loop_quit(m_mainLoop);
    }

    void clickAtPositionAndWaitUntilOptionMenuShown(int x, int y)
    {
        m_menu = nullptr;
        RunLoop::main().dispatch([this, x, y] { clickMouseButton(x, y); });
        g_main_loop_run(m_mainLoop);
    }

    void close()
    {
        g_assert_nonnull(m_menu.get());
        webkit_option_menu_close(m_menu.get());
        g_assert_null(m_menu.get());
    }

    void activateItem(unsigned item)
    {
        g_assert_nonnull(m_menu.get());
        webkit_option_menu_activate_item(m_menu.get(), item);
        g_assert_nonnull(m_menu.get());
    }

    void selectItem(unsigned item)
    {
        g_assert_nonnull(m_menu.get());
        webkit_option_menu_select_item(m_menu.get(), item);
        g_assert_nonnull(m_menu.get());
    }

    GRefPtr<WebKitOptionMenu> m_menu;
    PlatformRectangle m_rectangle;
};

static void testOptionMenuSimple(OptionMenuTest* test, gconstpointer)
{
    static const char html[] =
        "<html><body>"
        "  <select style='position:absolute; left:1; top:10'>"
        "    <option title='The Foo Option'>Foo</option>"
        "    <option selected>Bar</option>"
        "    <option disabled>Baz</option>"
        "  </select></body></html>";
    test->showInWindow();
    test->loadHtml(html, nullptr);
    test->waitUntilLoadFinished();

    test->clickAtPositionAndWaitUntilOptionMenuShown(5, 15);
    g_assert_true(WEBKIT_IS_OPTION_MENU(test->m_menu.get()));
    g_assert_cmpint(webkit_option_menu_get_n_items(test->m_menu.get()), ==, 3);
    auto* item = webkit_option_menu_get_item(test->m_menu.get(), 0);
    g_assert_cmpstr(webkit_option_menu_item_get_label(item), ==, "Foo");
    g_assert_cmpstr(webkit_option_menu_item_get_tooltip(item), ==, "The Foo Option");
    g_assert_false(webkit_option_menu_item_is_group_label(item));
    g_assert_false(webkit_option_menu_item_is_group_child(item));
    g_assert_true(webkit_option_menu_item_is_enabled(item));
    g_assert_false(webkit_option_menu_item_is_selected(item));
    item = webkit_option_menu_get_item(test->m_menu.get(), 1);
    g_assert_cmpstr(webkit_option_menu_item_get_label(item), ==, "Bar");
    g_assert_null(webkit_option_menu_item_get_tooltip(item));
    g_assert_false(webkit_option_menu_item_is_group_label(item));
    g_assert_false(webkit_option_menu_item_is_group_child(item));
    g_assert_true(webkit_option_menu_item_is_enabled(item));
    g_assert_true(webkit_option_menu_item_is_selected(item));
    item = webkit_option_menu_get_item(test->m_menu.get(), 2);
    g_assert_cmpstr(webkit_option_menu_item_get_label(item), ==, "Baz");
    g_assert_null(webkit_option_menu_item_get_tooltip(item));
    g_assert_false(webkit_option_menu_item_is_group_label(item));
    g_assert_false(webkit_option_menu_item_is_group_child(item));
    g_assert_false(webkit_option_menu_item_is_enabled(item));
    g_assert_false(webkit_option_menu_item_is_selected(item));

    test->close();
    g_assert_null(test->m_menu.get());
}

static void testOptionMenuGroups(OptionMenuTest* test, gconstpointer)
{
    static const char html[] =
        "<html><body>"
        "  <select style='position:absolute; left:1; top:10'>"
        "    <option>Root</option>"
        "    <optgroup label='Group 1'>"
        "      <option>Child 1-1</option>"
        "      <option disabled>Child 1-2</option>"
        "    </optgroup>"
        "    <optgroup label='Group 2'>"
        "      <option selected>Child 2-1</option>"
        "      <option>Child 2-2</option>"
        "    </optgroup>"
        "    <option>Tail</option>"
        "  </select></body></html>";
    test->showInWindow();
    test->loadHtml(html, nullptr);
    test->waitUntilLoadFinished();

    test->clickAtPositionAndWaitUntilOptionMenuShown(5, 15);
    g_assert_true(WEBKIT_IS_OPTION_MENU(test->m_menu.get()));
    g_assert_cmpint(webkit_option_menu_get_n_items(test->m_menu.get()), ==, 8);
    auto* item = webkit_option_menu_get_item(test->m_menu.get(), 0);
    g_assert_cmpstr(webkit_option_menu_item_get_label(item), ==, "Root");
    g_assert_null(webkit_option_menu_item_get_tooltip(item));
    g_assert_false(webkit_option_menu_item_is_group_label(item));
    g_assert_false(webkit_option_menu_item_is_group_child(item));
    g_assert_true(webkit_option_menu_item_is_enabled(item));
    g_assert_false(webkit_option_menu_item_is_selected(item));
    item = webkit_option_menu_get_item(test->m_menu.get(), 1);
    g_assert_cmpstr(webkit_option_menu_item_get_label(item), ==, "Group 1");
    g_assert_null(webkit_option_menu_item_get_tooltip(item));
    g_assert_true(webkit_option_menu_item_is_group_label(item));
    g_assert_false(webkit_option_menu_item_is_group_child(item));
    g_assert_false(webkit_option_menu_item_is_enabled(item));
    g_assert_false(webkit_option_menu_item_is_selected(item));
    item = webkit_option_menu_get_item(test->m_menu.get(), 2);
    g_assert_cmpstr(webkit_option_menu_item_get_label(item), ==, "Child 1-1");
    g_assert_null(webkit_option_menu_item_get_tooltip(item));
    g_assert_false(webkit_option_menu_item_is_group_label(item));
    g_assert_true(webkit_option_menu_item_is_group_child(item));
    g_assert_true(webkit_option_menu_item_is_enabled(item));
    g_assert_false(webkit_option_menu_item_is_selected(item));
    item = webkit_option_menu_get_item(test->m_menu.get(), 3);
    g_assert_cmpstr(webkit_option_menu_item_get_label(item), ==, "Child 1-2");
    g_assert_null(webkit_option_menu_item_get_tooltip(item));
    g_assert_false(webkit_option_menu_item_is_group_label(item));
    g_assert_true(webkit_option_menu_item_is_group_child(item));
    g_assert_false(webkit_option_menu_item_is_enabled(item));
    g_assert_false(webkit_option_menu_item_is_selected(item));
    item = webkit_option_menu_get_item(test->m_menu.get(), 4);
    g_assert_cmpstr(webkit_option_menu_item_get_label(item), ==, "Group 2");
    g_assert_null(webkit_option_menu_item_get_tooltip(item));
    g_assert_true(webkit_option_menu_item_is_group_label(item));
    g_assert_false(webkit_option_menu_item_is_group_child(item));
    g_assert_false(webkit_option_menu_item_is_enabled(item));
    g_assert_false(webkit_option_menu_item_is_selected(item));
    item = webkit_option_menu_get_item(test->m_menu.get(), 5);
    g_assert_cmpstr(webkit_option_menu_item_get_label(item), ==, "Child 2-1");
    g_assert_null(webkit_option_menu_item_get_tooltip(item));
    g_assert_false(webkit_option_menu_item_is_group_label(item));
    g_assert_true(webkit_option_menu_item_is_group_child(item));
    g_assert_true(webkit_option_menu_item_is_enabled(item));
    g_assert_true(webkit_option_menu_item_is_selected(item));
    item = webkit_option_menu_get_item(test->m_menu.get(), 6);
    g_assert_cmpstr(webkit_option_menu_item_get_label(item), ==, "Child 2-2");
    g_assert_null(webkit_option_menu_item_get_tooltip(item));
    g_assert_false(webkit_option_menu_item_is_group_label(item));
    g_assert_true(webkit_option_menu_item_is_group_child(item));
    g_assert_true(webkit_option_menu_item_is_enabled(item));
    g_assert_false(webkit_option_menu_item_is_selected(item));
    item = webkit_option_menu_get_item(test->m_menu.get(), 7);
    g_assert_cmpstr(webkit_option_menu_item_get_label(item), ==, "Tail");
    g_assert_null(webkit_option_menu_item_get_tooltip(item));
    g_assert_false(webkit_option_menu_item_is_group_label(item));
    g_assert_false(webkit_option_menu_item_is_group_child(item));
    g_assert_true(webkit_option_menu_item_is_enabled(item));
    g_assert_false(webkit_option_menu_item_is_selected(item));
}

static void testOptionMenuActivate(OptionMenuTest* test, gconstpointer)
{
    static const char html[] =
        "<html><body>"
        "  <select id='combo' style='position:absolute; left:1; top:10'>"
        "    <option>Foo</option>"
        "    <option>Bar</option>"
        "    <option>Baz</option>"
        "  </select></body></html>";
    test->showInWindow();
    test->loadHtml(html, nullptr);
    test->waitUntilLoadFinished();

    test->clickAtPositionAndWaitUntilOptionMenuShown(5, 15);
    g_assert_true(WEBKIT_IS_OPTION_MENU(test->m_menu.get()));
    g_assert_cmpint(webkit_option_menu_get_n_items(test->m_menu.get()), ==, 3);
    test->activateItem(1);
    auto* result = test->runJavaScriptAndWaitUntilFinished("document.getElementById('combo').selectedIndex", nullptr);
    g_assert_nonnull(result);
    g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(result), ==, 1);

    // We should close the menu after activate, further activates will be ignored.
    test->activateItem(2);
    result = test->runJavaScriptAndWaitUntilFinished("document.getElementById('combo').selectedIndex", nullptr);
    g_assert_nonnull(result);
    g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(result), ==, 1);

    test->close();
}

static void testOptionMenuSelect(OptionMenuTest* test, gconstpointer)
{
    static const char html[] =
        "<html><body>"
        "  <select id='combo' style='position:absolute; left:1; top:10'>"
        "    <option>Foo</option>"
        "    <option>Bar</option>"
        "    <option>Baz</option>"
        "  </select></body></html>";
    test->showInWindow();
    test->loadHtml(html, nullptr);
    test->waitUntilLoadFinished();

    test->clickAtPositionAndWaitUntilOptionMenuShown(5, 15);
    g_assert_true(WEBKIT_IS_OPTION_MENU(test->m_menu.get()));
    g_assert_cmpint(webkit_option_menu_get_n_items(test->m_menu.get()), ==, 3);

    // Select item changes the combo text, but not the currently selected item.
    test->selectItem(2);
    auto* result = test->runJavaScriptAndWaitUntilFinished("document.getElementById('combo').selectedIndex", nullptr);
    g_assert_nonnull(result);
    g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(result), ==, 0);

    // It can be called multiple times.
    test->selectItem(1);
    result = test->runJavaScriptAndWaitUntilFinished("document.getElementById('combo').selectedIndex", nullptr);
    g_assert_nonnull(result);
    g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(result), ==, 0);

    // And closing the menu activates the currently selected item.
    test->close();
    result = test->runJavaScriptAndWaitUntilFinished("document.getElementById('combo').selectedIndex", nullptr);
    g_assert_nonnull(result);
    g_assert_cmpfloat(WebViewTest::javascriptResultToNumber(result), ==, 1);
}

void beforeAll()
{
    OptionMenuTest::add("WebKitWebView", "option-menu-simple", testOptionMenuSimple);
    OptionMenuTest::add("WebKitWebView", "option-menu-groups", testOptionMenuGroups);
    OptionMenuTest::add("WebKitWebView", "option-menu-activate", testOptionMenuActivate);
    OptionMenuTest::add("WebKitWebView", "option-menu-select", testOptionMenuSelect);
}

void afterAll()
{
}
