/*
 * Copyright (C) 2009 Martin Robinson
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

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <glib/gstdio.h>
#include <webkit/webkit.h>

#if GTK_CHECK_VERSION(2, 14, 0)

typedef struct {
  char* page;
  gboolean shouldBeHandled;
} TestInfo;

typedef struct {
    GtkWidget* window;
    WebKitWebView* webView;
    GMainLoop* loop;
    TestInfo* info;
} KeyEventFixture;

TestInfo*
test_info_new(const char* page, gboolean shouldBeHandled)
{
    TestInfo* info;

    info = g_slice_new(TestInfo);
    info->page = g_strdup(page);
    info->shouldBeHandled = shouldBeHandled;

    return info;
}

void
test_info_destroy(TestInfo* info)
{
    g_free(info->page);
    g_slice_free(TestInfo, info);
}

static void key_event_fixture_setup(KeyEventFixture* fixture, gconstpointer data)
{
    fixture->loop = g_main_loop_new(NULL, TRUE);

    fixture->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    fixture->webView = WEBKIT_WEB_VIEW(webkit_web_view_new());

    gtk_container_add(GTK_CONTAINER(fixture->window), GTK_WIDGET(fixture->webView));
}

static void key_event_fixture_teardown(KeyEventFixture* fixture, gconstpointer data)
{
    gtk_widget_destroy(fixture->window);
    g_main_loop_unref(fixture->loop);
    test_info_destroy(fixture->info);
}

static gboolean key_press_event_cb(WebKitWebView* webView, GdkEvent* event, gpointer data)
{
    KeyEventFixture* fixture = (KeyEventFixture*)data;
    gboolean handled = GTK_WIDGET_GET_CLASS(fixture->webView)->key_press_event(GTK_WIDGET(fixture->webView), &event->key);
    g_assert_cmpint(handled, ==, fixture->info->shouldBeHandled);

    return FALSE;
}


static gboolean key_release_event_cb(WebKitWebView* webView, GdkEvent* event, gpointer data)
{
    // WebCore never seems to mark keyup events as handled.
    KeyEventFixture* fixture = (KeyEventFixture*)data;
    gboolean handled = GTK_WIDGET_GET_CLASS(fixture->webView)->key_press_event(GTK_WIDGET(fixture->webView), &event->key);
    g_assert(!handled);

    g_main_loop_quit(fixture->loop);

    return FALSE;
}

static void load_status_cb(WebKitWebView* webView, GParamSpec* spec, gpointer data)
{
    KeyEventFixture* fixture = (KeyEventFixture*)data;
    WebKitLoadStatus status = webkit_web_view_get_load_status(webView);
    if (status == WEBKIT_LOAD_FINISHED) {
        gtk_test_widget_send_key(GTK_WIDGET(fixture->webView),
                                 gdk_unicode_to_keyval('a'), 0);
    }

}

gboolean map_event_cb(GtkWidget *widget, GdkEvent* event, gpointer data)
{
    gtk_widget_grab_focus(widget);
    KeyEventFixture* fixture = (KeyEventFixture*)data;

    g_signal_connect(fixture->webView, "key-press-event",
                     G_CALLBACK(key_press_event_cb), fixture);
    g_signal_connect(fixture->webView, "key-release-event",
                     G_CALLBACK(key_release_event_cb), fixture);

    g_signal_connect(fixture->webView, "notify::load-status",
                     G_CALLBACK(load_status_cb), fixture);

    webkit_web_view_load_string(fixture->webView, fixture->info->page,
                                "text/html", "utf-8", "file://");

    return FALSE;
}

static void test_keypress(KeyEventFixture* fixture, gconstpointer data)
{
    fixture->info = (TestInfo*)data;

    g_signal_connect(fixture->window, "map-event",
                     G_CALLBACK(map_event_cb), fixture);

    gtk_widget_show(fixture->window);
    gtk_widget_show(GTK_WIDGET(fixture->webView));
    gtk_window_present(GTK_WINDOW(fixture->window));

    g_main_loop_run(fixture->loop);

}

int main(int argc, char** argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");

    g_test_add("/webkit/keyevent/textfield", KeyEventFixture,
               test_info_new("<html><body><input id=\"in\" type=\"text\">"
                             "<script>document.getElementById('in').focus();"
                             "</script></body></html>", TRUE),
               key_event_fixture_setup,
               test_keypress,
               key_event_fixture_teardown);

    g_test_add("/webkit/keyevent/buttons", KeyEventFixture,
               test_info_new("<html><body><input id=\"in\" type=\"button\">"
                             "<script>document.getElementById('in').focus();"
                             "</script></body></html>", FALSE),
               key_event_fixture_setup,
               test_keypress,
               key_event_fixture_teardown);

    g_test_add("/webkit/keyevent/link", KeyEventFixture,
               test_info_new("<html><body><a href=\"http://www.gnome.org\" id=\"in\">"
                             "LINKY MCLINKERSON</a><script>"
                             "document.getElementById('in').focus();</script>"
                             "</body></html>", FALSE),
               key_event_fixture_setup,
               test_keypress,
               key_event_fixture_teardown);

    return g_test_run();
}

#else

int main(int argc, char** argv)
{
    g_critical("You will need at least GTK+ 2.14.0 to run the unit tests.");
    return 0;
}

#endif
