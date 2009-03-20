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

int main(int argc, char** argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add_func("/webkit/webview/create_destroy", test_webkit_web_frame_create_destroy);
    g_test_add_func("/webkit/webframe/lifetime", test_webkit_web_frame_lifetime);
    return g_test_run ();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
