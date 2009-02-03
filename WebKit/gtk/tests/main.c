/*
 * Copyright (C) 2008 Holger Hans Peter Freyther
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

#include <glib.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#if GLIB_CHECK_VERSION(2, 16, 0) && GTK_CHECK_VERSION(2, 14, 0)

static void test_webkit_web_frame_create_destroy(void)
{
    WebKitWebView* webView;
    g_test_bug("21837");

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    g_assert_cmpint(G_OBJECT(webView)->ref_count, ==, 1);

    // This crashed with the original version
    g_object_unref(webView);
}

static void test_webkit_web_frame_lifetime(void)
{
    WebKitWebView* webView;
    WebKitWebFrame* webFrame;
    g_test_bug("21837");

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    g_assert_cmpint(G_OBJECT(webView)->ref_count, ==, 1);
    webFrame = webkit_web_view_get_main_frame(webView);
    g_assert_cmpint(G_OBJECT(webFrame)->ref_count, ==, 1);

    // Add dummy reference on the WebKitWebFrame to keep it alive
    g_object_ref(webFrame);
    g_assert_cmpint(G_OBJECT(webFrame)->ref_count, ==, 2);

    // This crashed with the original version
    g_object_unref(webView);

    // Make sure that the frame got deleted as well. We did this
    // by adding an extra ref on the WebKitWebFrame and we should
    // be the one holding the last reference.
    g_assert_cmpint(G_OBJECT(webFrame)->ref_count, ==, 1);
    g_object_unref(webFrame);
}

static void
test_webkit_web_back_forward_list_add_item(void)
{
    WebKitWebView* webView;
    WebKitWebBackForwardList* webBackForwardList;
    WebKitWebHistoryItem* addItem1;
    WebKitWebHistoryItem* addItem2;
    WebKitWebHistoryItem* backItem;
    WebKitWebHistoryItem* currentItem;
    g_test_bug("22988");

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);

    webkit_web_view_set_maintains_back_forward_list(webView, TRUE);
    webBackForwardList = webkit_web_view_get_back_forward_list(webView);
    g_assert(webBackForwardList);

    // Check that there is no item.
    g_assert(!webkit_web_back_forward_list_get_current_item(webBackForwardList));
    g_assert_cmpint(webkit_web_back_forward_list_get_forward_length(webBackForwardList), ==, 0);
    g_assert_cmpint(webkit_web_back_forward_list_get_back_length(webBackForwardList), ==, 0);
    g_assert(!webkit_web_view_can_go_forward(webView));
    g_assert(!webkit_web_view_can_go_back(webView));

    // Add a new item
    addItem1 = webkit_web_history_item_new_with_data("http://example.com/", "Added site");
    webkit_web_back_forward_list_add_item(webBackForwardList, addItem1);
    g_assert(webkit_web_back_forward_list_contains_item(webBackForwardList, addItem1));

    // Check that the added item is the current item.
    currentItem = webkit_web_back_forward_list_get_current_item(webBackForwardList);
    g_assert(currentItem);
    g_assert_cmpint(webkit_web_back_forward_list_get_forward_length(webBackForwardList), ==, 0);
    g_assert_cmpint(webkit_web_back_forward_list_get_back_length(webBackForwardList), ==, 0);
    g_assert(!webkit_web_view_can_go_forward(webView));
    g_assert(!webkit_web_view_can_go_back(webView));
    g_assert_cmpstr(webkit_web_history_item_get_uri(currentItem), ==, "http://example.com/");
    g_assert_cmpstr(webkit_web_history_item_get_title(currentItem), ==, "Added site");

    // Add another item.
    addItem2 = webkit_web_history_item_new_with_data("http://example.com/2/", "Added site 2");
    webkit_web_back_forward_list_add_item(webBackForwardList, addItem2);
    g_assert(webkit_web_back_forward_list_contains_item(webBackForwardList, addItem2));
    g_object_unref(addItem2);

    // Check that the added item is new current item.
    currentItem = webkit_web_back_forward_list_get_current_item(webBackForwardList);
    g_assert(currentItem);
    g_assert_cmpint(webkit_web_back_forward_list_get_forward_length(webBackForwardList), ==, 0);
    g_assert_cmpint(webkit_web_back_forward_list_get_back_length(webBackForwardList), ==, 1);
    g_assert(!webkit_web_view_can_go_forward(webView));
    g_assert(webkit_web_view_can_go_back(webView));
    g_assert_cmpstr(webkit_web_history_item_get_uri(currentItem), ==, "http://example.com/2/");
    g_assert_cmpstr(webkit_web_history_item_get_title(currentItem), ==, "Added site 2");

    backItem = webkit_web_back_forward_list_get_back_item(webBackForwardList);
    g_assert(backItem);
    g_assert_cmpstr(webkit_web_history_item_get_uri(backItem), ==, "http://example.com/");
    g_assert_cmpstr(webkit_web_history_item_get_title(backItem), ==, "Added site");

    // Go to the first added item.
    g_assert(webkit_web_view_go_to_back_forward_item(webView, addItem1));
    g_assert_cmpint(webkit_web_back_forward_list_get_forward_length(webBackForwardList), ==, 1);
    g_assert_cmpint(webkit_web_back_forward_list_get_back_length(webBackForwardList), ==, 0);
    g_assert(webkit_web_view_can_go_forward(webView));
    g_assert(!webkit_web_view_can_go_back(webView));

    g_object_unref(addItem1);
    g_object_unref(webView);
}

int main(int argc, char** argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add_func("/webkit/webview/create_destroy", test_webkit_web_frame_create_destroy);
    g_test_add_func("/webkit/webframe/lifetime", test_webkit_web_frame_lifetime);
    g_test_add_func("/webkit/webbackforwardlist/add_item", test_webkit_web_back_forward_list_add_item);

    return g_test_run ();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
