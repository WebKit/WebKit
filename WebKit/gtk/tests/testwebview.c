/*
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2009 Collabora Ltd.
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

#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#if GLIB_CHECK_VERSION(2, 16, 0) && GTK_CHECK_VERSION(2, 14, 0)

GMainLoop* loop;
SoupSession *session;
char* base_uri;

/* For real request testing */
static void
server_callback(SoupServer* server, SoupMessage* msg,
                 const char* path, GHashTable* query,
                 SoupClientContext* context, gpointer data)
{
    if (msg->method != SOUP_METHOD_GET) {
        soup_message_set_status(msg, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    soup_message_set_status(msg, SOUP_STATUS_OK);

    if (g_str_equal(path, "/favicon.ico")) {
        char* contents;
        gsize length;
        GError* error = NULL;

        g_file_get_contents("blank.ico", &contents, &length, &error);
        g_assert(!error);

        soup_message_body_append(msg->response_body, SOUP_MEMORY_TAKE, contents, length);
    } else {
        char* contents = g_strdup("<html><body>test</body></html>");
        soup_message_body_append(msg->response_body, SOUP_MEMORY_TAKE, contents, strlen(contents));
    }

    soup_message_body_complete(msg->response_body);
}

static gboolean idle_quit_loop_cb(gpointer data)
{
    g_main_loop_quit(loop);
    return FALSE;
}

static void icon_uri_changed_cb(WebKitWebView* web_view, GParamSpec* pspec, gpointer data)
{
    gboolean* been_here = (gboolean*)data;
    char* expected_uri;

    g_assert_cmpstr(g_param_spec_get_name(pspec), ==, "icon-uri");

    expected_uri = g_strdup_printf("%sfavicon.ico", base_uri);
    g_assert_cmpstr(webkit_web_view_get_icon_uri(web_view), ==, expected_uri);
    g_free(expected_uri);

    *been_here = TRUE;
}

static void icon_loaded_cb(WebKitWebView* web_view, char* icon_uri, gpointer data)
{
    gboolean* been_here = (gboolean*)data;
    char* expected_uri = g_strdup_printf("%sfavicon.ico", base_uri);
    g_assert_cmpstr(icon_uri, ==, expected_uri);
    g_free(expected_uri);

    g_assert_cmpstr(icon_uri, ==, webkit_web_view_get_icon_uri(web_view));

    *been_here = TRUE;
}

static void test_webkit_web_view_icon_uri()
{
    gboolean been_to_uri_changed = FALSE;
    gboolean been_to_icon_loaded = FALSE;
    WebKitWebView* view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(G_OBJECT(view));

    loop = g_main_loop_new(NULL, TRUE);

    g_object_connect(G_OBJECT(view),
                     "signal::load-finished", idle_quit_loop_cb, NULL,
                     "signal::notify::icon-uri", icon_uri_changed_cb, &been_to_uri_changed,
                     "signal::icon-loaded", icon_loaded_cb, &been_to_icon_loaded,
                     NULL);

    webkit_web_view_load_uri(view, base_uri);

    g_main_loop_run(loop);

    g_assert(been_to_uri_changed);
    g_assert(been_to_icon_loaded);

    g_object_unref(view);
}

int main(int argc, char** argv)
{
    SoupServer* server;
    SoupURI* soup_uri;
    char* test_dir;
    char* resources_dir;

    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    /* Hopefully make test independent of the path it's called from. */
    test_dir = g_path_get_dirname(argv[0]);
    resources_dir = g_build_path(G_DIR_SEPARATOR_S, test_dir,
                                 "..", "..", "..", "..",
                                 "WebKit", "gtk", "tests", "resources",
                                 NULL);
    g_free(test_dir);

    g_chdir(resources_dir);
    g_free(resources_dir);

    server = soup_server_new(SOUP_SERVER_PORT, 0, NULL);
    soup_server_run_async(server);

    soup_server_add_handler(server, NULL, server_callback, NULL, NULL);

    soup_uri = soup_uri_new("http://127.0.0.1/");
    soup_uri_set_port(soup_uri, soup_server_get_port(server));

    base_uri = soup_uri_to_string(soup_uri, FALSE);
    soup_uri_free(soup_uri);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add_func("/webkit/webview/icon-uri", test_webkit_web_view_icon_uri);

    return g_test_run ();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
