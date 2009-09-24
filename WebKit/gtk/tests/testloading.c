/*
 * Copyright (C) 2009 Gustavo Noronha Silva
 * Copyright (C) 2009 Igalia S.L.
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

typedef struct {
    WebKitWebView* webView;
    GMainLoop *loop;
    gboolean has_been_provisional;
    gboolean has_been_committed;
    gboolean has_been_first_visually_non_empty_layout;
    gboolean has_been_finished;
    gboolean has_been_failed;
    gboolean has_been_load_error;
} WebLoadingFixture;

static void web_loading_fixture_setup(WebLoadingFixture* fixture, gconstpointer data)
{
    fixture->webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    fixture->loop = g_main_loop_new(NULL, TRUE);
    g_object_ref_sink(fixture->webView);
    fixture->has_been_provisional = FALSE;
    fixture->has_been_committed = FALSE;
    fixture->has_been_first_visually_non_empty_layout = FALSE;
    fixture->has_been_finished = FALSE;
    fixture->has_been_failed = FALSE;
    fixture->has_been_load_error = FALSE;
}

static void web_loading_fixture_teardown(WebLoadingFixture* fixture, gconstpointer data)
{
    g_object_unref(fixture->webView);
    g_main_loop_unref(fixture->loop);
}

static void load_finished_cb(WebKitWebView* web_view, WebKitWebFrame* web_frame, WebLoadingFixture* fixture)
{
    g_assert(fixture->has_been_provisional);
    g_assert(fixture->has_been_committed);
    g_assert(fixture->has_been_first_visually_non_empty_layout);

    g_main_loop_quit(fixture->loop);
}


static void status_changed_cb(GObject* object, GParamSpec* pspec, WebLoadingFixture* fixture)
{
    WebKitLoadStatus status = webkit_web_view_get_load_status(WEBKIT_WEB_VIEW(object));

    switch (status) {
    case WEBKIT_LOAD_PROVISIONAL:
        g_assert(!fixture->has_been_provisional);
        g_assert(!fixture->has_been_committed);
        g_assert(!fixture->has_been_first_visually_non_empty_layout);
        fixture->has_been_provisional = TRUE;
        break;
    case WEBKIT_LOAD_COMMITTED:
        g_assert(fixture->has_been_provisional);
        g_assert(!fixture->has_been_committed);
        g_assert(!fixture->has_been_first_visually_non_empty_layout);
        fixture->has_been_committed = TRUE;
        break;
    case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT:
        g_assert(fixture->has_been_provisional);
        g_assert(fixture->has_been_committed);
        g_assert(!fixture->has_been_first_visually_non_empty_layout);
        fixture->has_been_first_visually_non_empty_layout = TRUE;
        break;
    case WEBKIT_LOAD_FINISHED:
        g_assert(fixture->has_been_provisional);
        g_assert(fixture->has_been_committed);
        g_assert(fixture->has_been_first_visually_non_empty_layout);
        break;
    default:
        g_assert_not_reached();
    }
}

static void test_loading_status(WebLoadingFixture* fixture, gconstpointer data)
{
    g_assert_cmpint(webkit_web_view_get_load_status(fixture->webView), ==, WEBKIT_LOAD_PROVISIONAL);

    g_object_connect(G_OBJECT(fixture->webView),
                     "signal::notify::load-status", G_CALLBACK(status_changed_cb), fixture,
                     "signal::load-finished", G_CALLBACK(load_finished_cb), fixture,
                     NULL);

    /* load_uri will trigger the navigation-policy-decision-requested
     * signal emission;
     */
    webkit_web_view_load_uri(fixture->webView, "http://gnome.org/");

    g_main_loop_run(fixture->loop);
}

static void load_error_status_changed_cb(GObject* object, GParamSpec* pspec, WebLoadingFixture* fixture)
{
    WebKitLoadStatus status = webkit_web_view_get_load_status(WEBKIT_WEB_VIEW(object));

    switch(status) {
    case WEBKIT_LOAD_PROVISIONAL:
        /* We are going to go through here twice, so don't assert
         * anything */
        fixture->has_been_provisional = TRUE;
        break;
    case WEBKIT_LOAD_FINISHED:
        g_assert(fixture->has_been_provisional);
        g_assert(fixture->has_been_load_error);
        g_assert(fixture->has_been_failed);
        /* We are checking that only one FINISHED is received in the
           whole cycle, so assert it's FALSE */
        g_assert(!fixture->has_been_finished);
        fixture->has_been_finished = TRUE;
        g_main_loop_quit(fixture->loop);
        break;
    case WEBKIT_LOAD_FAILED:
        g_assert(!fixture->has_been_failed);
        fixture->has_been_failed = TRUE;
        break;
    default:
        break;
    }
}

static gboolean load_error_cb(WebKitWebView* webView, WebKitWebFrame* frame, const char* uri, GError *error, WebLoadingFixture* fixture)
{
    g_assert(fixture->has_been_provisional);
    g_assert(!fixture->has_been_load_error);
    fixture->has_been_load_error = TRUE;

    return FALSE;
}

static void test_loading_error(WebLoadingFixture* fixture, gconstpointer data)
{
    g_test_bug("28842");

    g_signal_connect(fixture->webView, "load-error", G_CALLBACK(load_error_cb), fixture);
    g_signal_connect(fixture->webView, "notify::load-status", G_CALLBACK(load_error_status_changed_cb), fixture);

    webkit_web_view_load_uri(fixture->webView, "http://snoetuhsetuhseoutoeutc.com/");
    g_main_loop_run(fixture->loop);
}

/* Cancelled load */

static gboolean load_cancelled_cb(WebKitWebView* webView, WebKitWebFrame* frame, const char* uri, GError *error, WebLoadingFixture* fixture)
{
    g_assert(fixture->has_been_provisional);
    g_assert(fixture->has_been_failed);
    g_assert(!fixture->has_been_load_error);
    g_assert(error->code == WEBKIT_NETWORK_ERROR_CANCELLED);
    fixture->has_been_load_error = TRUE;

    return TRUE;
}

static gboolean stop_load (gpointer data)
{
    webkit_web_view_stop_loading(WEBKIT_WEB_VIEW(data));
    return FALSE;
}

static void load_cancelled_status_changed_cb(GObject* object, GParamSpec* pspec, WebLoadingFixture* fixture)
{
    WebKitLoadStatus status = webkit_web_view_get_load_status(WEBKIT_WEB_VIEW(object));

    switch(status) {
    case WEBKIT_LOAD_PROVISIONAL:
        g_assert(!fixture->has_been_provisional);
        g_assert(!fixture->has_been_failed);
        fixture->has_been_provisional = TRUE;
        break;
    case WEBKIT_LOAD_COMMITTED:
        g_idle_add (stop_load, object);
        break;
    case WEBKIT_LOAD_FAILED:
        g_assert(fixture->has_been_provisional);
        g_assert(!fixture->has_been_failed);
        g_assert(!fixture->has_been_load_error);
        fixture->has_been_failed = TRUE;
        g_main_loop_quit(fixture->loop);
        break;
    case WEBKIT_LOAD_FINISHED:
        g_assert_not_reached();
        break;
    default:
        break;
    }
}

static void test_loading_cancelled(WebLoadingFixture* fixture, gconstpointer data)
{
    g_test_bug("29644");

    g_signal_connect(fixture->webView, "load-error", G_CALLBACK(load_cancelled_cb), fixture);
    g_signal_connect(fixture->webView, "notify::load-status", G_CALLBACK(load_cancelled_status_changed_cb), fixture);

    webkit_web_view_load_uri(fixture->webView, "http://google.com/");
    g_main_loop_run(fixture->loop);
}

int main(int argc, char** argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add("/webkit/loading/status",
               WebLoadingFixture, NULL,
               web_loading_fixture_setup,
               test_loading_status,
               web_loading_fixture_teardown);
    g_test_add("/webkit/loading/error",
               WebLoadingFixture, NULL,
               web_loading_fixture_setup,
               test_loading_error,
               web_loading_fixture_teardown);
    g_test_add("/webkit/loading/cancelled",
               WebLoadingFixture, NULL,
               web_loading_fixture_setup,
               test_loading_cancelled,
               web_loading_fixture_teardown);
    return g_test_run();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
