/*
 * Copyright (C) 2009 Gustavo Noronha Silva
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

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#if GLIB_CHECK_VERSION(2, 16, 0) && GTK_CHECK_VERSION(2, 14, 0)

static gboolean has_been_provisional = FALSE;
static gboolean has_been_committed = FALSE;
static gboolean has_been_first_visually_non_empty_layout = FALSE;

static void load_finished_cb(WebKitWebView* web_view, WebKitWebFrame* web_frame, gpointer data)
{
    GMainLoop* loop = (GMainLoop*)data;

    g_assert(has_been_provisional);
    g_assert(has_been_committed);
    g_assert(has_been_first_visually_non_empty_layout);

    g_main_loop_quit(loop);
}


static void status_changed_cb(GObject* object, GParamSpec* pspec, gpointer data)
{
    WebKitLoadStatus status = webkit_web_view_get_load_status(WEBKIT_WEB_VIEW(object));

    switch (status) {
    case WEBKIT_LOAD_PROVISIONAL:
        g_assert(!has_been_provisional);
        g_assert(!has_been_committed);
        g_assert(!has_been_first_visually_non_empty_layout);
        has_been_provisional = TRUE;
        break;
    case WEBKIT_LOAD_COMMITTED:
        g_assert(has_been_provisional);
        g_assert(!has_been_committed);
        g_assert(!has_been_first_visually_non_empty_layout);
        has_been_committed = TRUE;
        break;
    case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT:
        g_assert(has_been_provisional);
        g_assert(has_been_committed);
        g_assert(!has_been_first_visually_non_empty_layout);
        has_been_first_visually_non_empty_layout = TRUE;
        break;
    case WEBKIT_LOAD_FINISHED:
        g_assert(has_been_provisional);
        g_assert(has_been_committed);
        g_assert(has_been_first_visually_non_empty_layout);
        break;
    default:
        g_assert_not_reached();
    }
}

static void test_loading_status()
{
    WebKitWebView* web_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    GMainLoop* loop = g_main_loop_new(NULL, TRUE);

    g_object_ref_sink(web_view);

    g_assert_cmpint(webkit_web_view_get_load_status(web_view), ==, WEBKIT_LOAD_PROVISIONAL);

    g_object_connect(G_OBJECT(web_view),
                     "signal::notify::load-status", G_CALLBACK(status_changed_cb), NULL,
                     "signal::load-finished", G_CALLBACK(load_finished_cb), loop,
                     NULL);

    /* load_uri will trigger the navigation-policy-decision-requested
     * signal emission;
     */
    webkit_web_view_load_uri(web_view, "http://gnome.org/");

    g_main_loop_run(loop);

    g_object_unref(web_view);
}

int main(int argc, char** argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add_func("/webkit/loading/status", test_loading_status);
    return g_test_run();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
