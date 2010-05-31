/*
 * Copyright (C) 2010 Igalia S.L.
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
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSContextRef.h>


#if GTK_CHECK_VERSION(2, 14, 0)

typedef struct {
    char* page;
    char* expectedPlainText;
} TestInfo;

typedef struct {
    GtkWidget* window;
    WebKitWebView* webView;
    GMainLoop* loop;
    TestInfo* info;
} CopyAndPasteFixture;

TestInfo*
test_info_new(const char* page, const char* expectedPlainText)
{
    TestInfo* info;
    info = g_slice_new0(TestInfo);
    info->page = g_strdup(page);
    if (expectedPlainText)
        info->expectedPlainText = g_strdup(expectedPlainText);
    return info;
}

void
test_info_destroy(TestInfo* info)
{
    g_free(info->page);
    g_free(info->expectedPlainText);
    g_slice_free(TestInfo, info);
}

static void copy_and_paste_fixture_setup(CopyAndPasteFixture* fixture, gconstpointer data)
{
    fixture->loop = g_main_loop_new(NULL, TRUE);

    fixture->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    fixture->webView = WEBKIT_WEB_VIEW(webkit_web_view_new());

    gtk_container_add(GTK_CONTAINER(fixture->window), GTK_WIDGET(fixture->webView));
}

static void copy_and_paste_fixture_teardown(CopyAndPasteFixture* fixture, gconstpointer data)
{
    gtk_widget_destroy(fixture->window);
    g_main_loop_unref(fixture->loop);
    test_info_destroy(fixture->info);
}

static void load_status_cb(WebKitWebView* webView, GParamSpec* spec, gpointer data)
{
    CopyAndPasteFixture* fixture = (CopyAndPasteFixture*)data;
    WebKitLoadStatus status = webkit_web_view_get_load_status(webView);
    if (status != WEBKIT_LOAD_FINISHED)
        return;

    GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_clear(clipboard);

    webkit_web_view_copy_clipboard(webView);

    gchar* text = gtk_clipboard_wait_for_text(clipboard);
    g_assert(text || !fixture->info->expectedPlainText);
    g_assert(!text || !strcmp(text, fixture->info->expectedPlainText));
    g_free(text);

    g_assert(!gtk_clipboard_wait_is_uris_available(clipboard));
    g_assert(!gtk_clipboard_wait_is_image_available(clipboard));

    g_main_loop_quit(fixture->loop);
}

gboolean map_event_cb(GtkWidget *widget, GdkEvent* event, gpointer data)
{
    gtk_widget_grab_focus(widget);
    CopyAndPasteFixture* fixture = (CopyAndPasteFixture*)data;
    webkit_web_view_load_string(fixture->webView, fixture->info->page,
                                "text/html", "utf-8", "file://");
    return FALSE;
}

static void test_copy_and_paste(CopyAndPasteFixture* fixture, gconstpointer data)
{
    fixture->info = (TestInfo*)data;
    g_signal_connect(fixture->window, "map-event",
                     G_CALLBACK(map_event_cb), fixture);

    gtk_widget_show(fixture->window);
    gtk_widget_show(GTK_WIDGET(fixture->webView));
    gtk_window_present(GTK_WINDOW(fixture->window));

    g_signal_connect(fixture->webView, "notify::load-status",
                     G_CALLBACK(load_status_cb), fixture);

    g_main_loop_run(fixture->loop);
}

int main(int argc, char** argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");
    const char* selected_span_html = "<html><body>"
        "<span id=\"mainspan\">All work and no play <span>make Jack a dull</span> boy.</span>"
        "<script>document.getSelection().collapse();\n"
        "document.getSelection().selectAllChildren(document.getElementById('mainspan'));\n"
        "</script></body></html>";
    const char* no_selection_html = "<html><body>"
        "<span id=\"mainspan\">All work and no play <span>make Jack a dull</span> boy</span>"
        "<script>document.getSelection().collapse();\n"
        "</script></body></html>";

    g_test_add("/webkit/copyandpaste/selection", CopyAndPasteFixture,
               test_info_new(selected_span_html, "All work and no play make Jack a dull boy."),
               copy_and_paste_fixture_setup,
               test_copy_and_paste,
               copy_and_paste_fixture_teardown);
    g_test_add("/webkit/copyandpaste/no-selection", CopyAndPasteFixture,
               test_info_new(no_selection_html, 0),
               copy_and_paste_fixture_setup,
               test_copy_and_paste,
               copy_and_paste_fixture_teardown);

    return g_test_run();
}

#else

int main(int argc, char** argv)
{
    g_critical("You will need at least GTK+ 2.14.0 to run the unit tests.");
    return 0;
}

#endif
