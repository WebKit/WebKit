/*
 * Copyright (C) 2011 Lukasz Slachciak
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

#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#if GTK_CHECK_VERSION(2, 14, 0)

static void test_application_cache_maximum_size()
{
    unsigned long long maxSize = 8192;
    webkit_application_cache_set_maximum_size(maxSize);

    // Creating a WebView - make sure that it didn't change anything
    WebKitWebView* webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    g_object_unref(webView);

    g_assert(maxSize == webkit_application_cache_get_maximum_size());
}

static void test_application_cache_database_directory_path()
{
    unsigned long long maxSize = 8192;
    webkit_application_cache_set_maximum_size(maxSize);

    gchar* databaseDirectorySet = g_build_filename(g_get_user_data_dir(), "webkit", "databases", NULL);
    webkit_application_cache_set_database_directory_path(databaseDirectorySet);

    // Creating a WebView - make sure that it didn't change anything
    WebKitWebView* webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    g_object_unref(webView);

    const gchar* databaseDirectoryGet = webkit_application_cache_get_database_directory_path();
    g_assert_cmpstr(databaseDirectorySet, ==, databaseDirectoryGet);

    g_free(databaseDirectorySet);
}

int main(int argc, char** argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add_func("/webkit/application_cache/maximum_size",
                    test_application_cache_maximum_size);
    g_test_add_func("/webkit/application_cache/database_directory_path",
                    test_application_cache_database_directory_path);

    return g_test_run();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
