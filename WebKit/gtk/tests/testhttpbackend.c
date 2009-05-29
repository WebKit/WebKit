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

#include <errno.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#if GLIB_CHECK_VERSION(2, 16, 0) && GTK_CHECK_VERSION(2, 14, 0)

SoupMessage* message;

// Not yet public API
SoupMessage* webkit_network_request_get_message(WebKitNetworkRequest* request);

static gboolean navigation_policy_decision_requested_cb(WebKitWebView* web_view,
                                                        WebKitWebFrame* web_frame,
                                                        WebKitNetworkRequest* request,
                                                        WebKitWebNavigationAction action,
                                                        gpointer data)
{
    message = webkit_network_request_get_message(request);

    /* 1 -> soup_message_new_from_uri
     * 2 -> g_object_ref
     * 3 -> webkit_network_request_with_core_request
     *
     * 1, and 2 are caused by the creation of the ResourceRequest
     * object; 3 is caused by the emission of this signal!
     */
    g_assert_cmpint(G_OBJECT(message)->ref_count, ==, 2);

    return FALSE;
}

static void load_finished_cb(WebKitWebView* web_view, WebKitWebFrame* web_frame, GMainLoop* loop)
{
    /* 1 -> ResourceRequest's
     * 2 -> ResourceHandle's (ref in startHttp)
     *
     * At this point, although the SoupSession dropped its reference,
     * ResourceHandle is still there, as is the ResourceRequest, so we
     * still have 2.
     */
    g_assert_cmpint(G_OBJECT(message)->ref_count, ==, 1);
    g_main_loop_quit(loop);
}

static void test_soup_message_lifetime()
{
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    WebKitWebView* web_view = WEBKIT_WEB_VIEW(webkit_web_view_new());

    g_object_ref_sink(web_view);

    g_signal_connect(web_view, "navigation-policy-decision-requested",
                     G_CALLBACK(navigation_policy_decision_requested_cb),
                     NULL);

    g_signal_connect(web_view, "load-finished",
                     G_CALLBACK(load_finished_cb),
                     loop);

    /* load_uri will trigger the navigation-policy-decision-requested
     * signal emission;
     */
    webkit_web_view_load_uri(web_view, "http://127.0.0.1/");

    /* to wait for load-finished */
    g_main_loop_run(loop);

    /* keep our own reference 2->3 */
    g_object_ref(message);
    g_assert_cmpint(G_OBJECT(message)->ref_count, ==, 2);

    /* this should make the ResourceHandle and ResourceRequest go
     * away, so we will go down to 1
     */
    webkit_web_view_load_uri(web_view, "about:blank");
    g_object_unref(web_view);

    g_assert_cmpint(G_OBJECT(message)->ref_count, ==, 1);
    g_object_unref(message);
}

int main(int argc, char** argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add_func("/webkit/soupmessage/lifetime", test_soup_message_lifetime);
    return g_test_run ();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
