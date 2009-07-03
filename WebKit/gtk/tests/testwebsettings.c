/*
 * Copyright (C) 2009 Jan Michael Alonzo
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

static void test_webkit_web_settings_user_agent(void)
{
    WebKitWebSettings* settings;
    WebKitWebView* webView;
    gchar* defaultUserAgent;
    gchar* userAgent;
    g_test_bug("17375");

    webView = webkit_web_view_new();
    g_object_ref_sink(webView);

    settings = webkit_web_view_get_settings(webView);
    defaultUserAgent = g_strdup(webkit_web_settings_get_user_agent(settings));

    // test a custom UA string
    userAgent = NULL;
    g_object_set(G_OBJECT(settings), "user-agent", "testwebsettings/0.1", NULL);
    g_object_get(G_OBJECT(settings),"user-agent", &userAgent, NULL);
    g_assert_cmpstr(userAgent, ==, "testwebsettings/0.1");
    g_free(userAgent);

    // setting it to NULL or an empty value should give us the default UA string
    userAgent = NULL;
    g_object_set(G_OBJECT(settings), "user-agent", NULL, NULL);
    g_object_get(G_OBJECT(settings),"user-agent", &userAgent, NULL);
    g_assert_cmpstr(userAgent, ==, defaultUserAgent);
    g_free(userAgent);

    userAgent = NULL;
    g_object_set(G_OBJECT(settings), "user-agent", "", NULL);
    g_object_get(G_OBJECT(settings),"user-agent", &userAgent, NULL);
    g_assert_cmpstr(userAgent, ==, defaultUserAgent);
    g_free(userAgent);

    g_free(defaultUserAgent);
    g_object_unref(webView);
}

int main(int argc, char** argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add_func("/webkit/websettings/user_agent", test_webkit_web_settings_user_agent);
    return g_test_run ();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
