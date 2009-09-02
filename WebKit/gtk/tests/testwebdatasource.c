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

static const gshort defaultTimeout = 10;
guint waitTimer;
gboolean shouldWait;

typedef struct {
    WebKitWebView* webView;
    WebKitWebFrame* mainFrame;
} WebDataSourceFixture;

static void test_webkit_web_data_source_get_initial_request()
{
    WebKitWebView* view;
    WebKitWebFrame* frame;
    WebKitWebDataSource* dataSource;
    WebKitNetworkRequest* initialRequest;

    view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(view);
    frame = webkit_web_view_get_main_frame(view);

    WebKitNetworkRequest* request = webkit_network_request_new("http://www.google.com");
    webkit_web_frame_load_request(frame, request);
    g_object_unref(request);

    dataSource = webkit_web_frame_get_provisional_data_source(frame);
    g_assert(dataSource);
    initialRequest = webkit_web_data_source_get_initial_request(dataSource);
    g_assert_cmpstr(webkit_network_request_get_uri(initialRequest), ==, "http://www.google.com/");

    g_object_unref(view);
}

static void load_finished_unreachable_cb(WebKitWebView* view, WebKitWebFrame* frame, GMainLoop* loop)
{
    if (waitTimer) {
        g_source_remove(waitTimer);
        waitTimer = 0;
    }

    WebKitWebDataSource* datasource;
    frame = webkit_web_view_get_main_frame(view);
    datasource = webkit_web_frame_get_data_source(frame);

    g_assert_cmpstr("http://localhost/doireallyexist.html", ==,
                    webkit_web_data_source_get_unreachable_uri(datasource));

    if (g_main_loop_is_running(loop))
        g_main_loop_quit(loop);
}

static void load_finished_cb(WebKitWebView* view, WebKitWebFrame* frame, GMainLoop* loop)
{
    if (waitTimer) {
        g_source_remove(waitTimer);
        waitTimer = 0;
    }

    WebKitWebDataSource* dataSource;
    frame = webkit_web_view_get_main_frame(view);
    dataSource = webkit_web_frame_get_data_source(frame);

    /* Test get_request */
    g_test_message("Testing webkit_web_data_source_get_request");
    WebKitNetworkRequest* request = webkit_web_data_source_get_request(dataSource);
    g_assert_cmpstr(webkit_network_request_get_uri(request), ==, "http://webkit.org/");

    /* Test get_main_resource */
    g_test_message("Testing webkit_web_data_source_get_main_resource");
    WebKitWebResource* resource = webkit_web_data_source_get_main_resource(dataSource);
    g_assert_cmpstr("text/html", ==, webkit_web_resource_get_mime_type(resource));
    g_assert_cmpstr("http://webkit.org/", ==, webkit_web_resource_get_uri(resource));

    /* Test get_data. We just test if data has certain size for the mean time */
    g_test_message("Testing webkit_web_data_source_get_data has certain size");
    GString* data = webkit_web_data_source_get_data(dataSource);
    g_assert(data->len > 100);

    /* FIXME: Add test for get_encoding */

    if (g_main_loop_is_running(loop))
        g_main_loop_quit(loop);
}

static void load_committed_cb(WebKitWebView* view, WebKitWebFrame* frame)
{
    WebKitWebDataSource* dataSource = webkit_web_frame_get_data_source(frame);
    g_assert(webkit_web_data_source_is_loading(dataSource));
}

static gboolean wait_timer_fired(GMainLoop* loop)
{
    waitTimer = 0;
    if (g_main_loop_is_running(loop))
        g_main_loop_quit(loop);

    return FALSE;
}

static void test_webkit_web_data_source()
{
    WebKitWebView* view;
    GMainLoop* loop;

    view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(view);
    loop = g_main_loop_new(NULL, TRUE);
    g_signal_connect(view, "load-committed", G_CALLBACK(load_committed_cb), loop);
    g_signal_connect(view, "load-finished", G_CALLBACK(load_finished_cb), loop);
    webkit_web_view_load_uri(view, "http://webkit.org");

    if (!waitTimer)
        waitTimer = g_timeout_add_seconds(defaultTimeout, (GSourceFunc)wait_timer_fired, loop);

    g_main_loop_run(loop);
    g_object_unref(view);
}

static void test_webkit_web_data_source_unreachable_uri()
{
    WebKitWebView* view;
    GMainLoop* loop;

    view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(view);
    loop = g_main_loop_new(NULL, TRUE);
    g_signal_connect(view, "load-finished", G_CALLBACK(load_finished_unreachable_cb), loop);
    webkit_web_view_load_uri(view, "http://localhost/doireallyexist.html");

    if (!waitTimer)
        waitTimer = g_timeout_add_seconds(defaultTimeout, (GSourceFunc)wait_timer_fired, loop);

    g_main_loop_run(loop);
    g_object_unref(view);
}

int main(int argc, char** argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_bug("24758");
    g_test_add_func("/webkit/webdatasource/get_initial_request",
                    test_webkit_web_data_source_get_initial_request);
    g_test_add_func("/webkit/webdatasource/api",
                    test_webkit_web_data_source);
    g_test_add_func("/webkit/webdatasource/unreachable_uri",
                    test_webkit_web_data_source_unreachable_uri);
    return g_test_run ();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
