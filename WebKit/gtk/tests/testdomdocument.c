/*
 * Copyright (C) 2010 Igalia S.L.
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

#include "test_utils.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#if GLIB_CHECK_VERSION(2, 16, 0) && GTK_CHECK_VERSION(2, 14, 0)

#define HTML_DOCUMENT_TITLE "<html><head><title>This is the title</title></head><body></body></html>"

typedef struct {
    GtkWidget* webView;
    GMainLoop* loop;
} DomDocumentFixture;

static gboolean finish_loading(DomDocumentFixture* fixture)
{
    if (g_main_loop_is_running(fixture->loop))
        g_main_loop_quit(fixture->loop);

    return FALSE;
}

static void dom_document_fixture_setup(DomDocumentFixture* fixture, gconstpointer data)
{
    fixture->loop = g_main_loop_new(NULL, TRUE);
    fixture->webView = webkit_web_view_new();
    g_object_ref_sink(fixture->webView);

    if (data != NULL)
        webkit_web_view_load_string(WEBKIT_WEB_VIEW (fixture->webView), (const char*) data, NULL, NULL, NULL);

    g_idle_add((GSourceFunc)finish_loading, fixture);
    g_main_loop_run(fixture->loop);
}

static void dom_document_fixture_teardown(DomDocumentFixture* fixture, gconstpointer data)
{
    g_object_unref(fixture->webView);
    g_main_loop_unref(fixture->loop);
}

static void test_dom_document_title(DomDocumentFixture* fixture, gconstpointer data)
{
    g_assert(fixture);
    WebKitWebView* view = (WebKitWebView*)fixture->webView;
    g_assert(view);
    WebKitDOMDocument* document = webkit_web_view_get_dom_document(view);
    g_assert(document);
    gchar* title = webkit_dom_document_get_title(document);
    g_assert(title);
    g_assert_cmpstr(title, ==, "This is the title");
    g_free(title);
    webkit_dom_document_set_title(document, (gchar*)"This is the second title");
    title = webkit_dom_document_get_title(document);
    g_assert(title);
    g_assert_cmpstr(title, ==, "This is the second title");
    g_free(title);
}

int main(int argc, char** argv)
{
    if (!g_thread_supported())
        g_thread_init(NULL);

    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");

    g_test_add("/webkit/domdocument/test_title",
               DomDocumentFixture, HTML_DOCUMENT_TITLE,
               dom_document_fixture_setup,
               test_dom_document_title,
               dom_document_fixture_teardown);

    return g_test_run();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
