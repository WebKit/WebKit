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
#include <string.h>
#include <webkit/webkit.h>

#if GLIB_CHECK_VERSION(2, 16, 0) && GTK_CHECK_VERSION(2, 14, 0)

typedef struct {
    WebKitWebResource* webResource;
    WebKitWebView* webView;
} WebResourceFixture;

static void web_resource_fixture_setup(WebResourceFixture* fixture, gconstpointer data)
{
    fixture->webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(fixture->webView);
    const gchar* webData = "<html></html>";
    fixture->webResource = webkit_web_resource_new(webData, strlen(webData), "http://example.com/", "text/html", "utf8", "Example.com");
    g_assert(fixture->webResource);
}

static void web_resource_fixture_teardown(WebResourceFixture* fixture, gconstpointer data)
{
    g_assert(fixture->webResource);
    g_object_unref(fixture->webResource);
    g_object_unref(fixture->webView);
}

static void test_webkit_web_resource_get_url(WebResourceFixture* fixture, gconstpointer data)
{
    gchar* url;
    g_object_get(G_OBJECT(fixture->webResource), "uri", &url, NULL);
    g_assert_cmpstr(url, ==, "http://example.com/");
    g_assert_cmpstr(webkit_web_resource_get_uri(fixture->webResource) ,==,"http://example.com/");
    g_free(url);
}

static void test_webkit_web_resource_get_data(WebResourceFixture* fixture, gconstpointer data)
{
    GString* charData = webkit_web_resource_get_data(fixture->webResource);
    g_assert_cmpstr(charData->str, ==, "<html></html>");
}

static void test_webkit_web_resource_get_mime_type(WebResourceFixture* fixture, gconstpointer data)
{
    gchar* mime_type;
    g_object_get(G_OBJECT(fixture->webResource), "mime-type", &mime_type, NULL);
    g_assert_cmpstr(mime_type, ==, "text/html");
    g_assert_cmpstr(webkit_web_resource_get_mime_type(fixture->webResource),==,"text/html");
    g_free(mime_type);
}

static void test_webkit_web_resource_get_encoding(WebResourceFixture* fixture, gconstpointer data)
{
    gchar* text_encoding;
    g_object_get(G_OBJECT(fixture->webResource), "encoding", &text_encoding, NULL);
    g_assert_cmpstr(text_encoding, ==, "utf8");
    g_assert_cmpstr(webkit_web_resource_get_encoding(fixture->webResource),==,"utf8");
    g_free(text_encoding);
}

static void test_webkit_web_resource_get_frame_name(WebResourceFixture* fixture, gconstpointer data)
{
    gchar* frame_name;
    g_object_get(G_OBJECT(fixture->webResource), "frame-name", &frame_name, NULL);
    g_assert_cmpstr(frame_name, ==, "Example.com");
    g_assert_cmpstr(webkit_web_resource_get_frame_name(fixture->webResource),==,"Example.com");
    g_free(frame_name);
}

int main(int argc, char** argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add("/webkit/webresource/get_url",
               WebResourceFixture, 0, web_resource_fixture_setup,
               test_webkit_web_resource_get_url, web_resource_fixture_teardown);
    g_test_add("/webkit/webresource/get_mime_type",
               WebResourceFixture, 0, web_resource_fixture_setup,
               test_webkit_web_resource_get_mime_type, web_resource_fixture_teardown);
    g_test_add("/webkit/webresource/get_text_encoding_name",
               WebResourceFixture, 0, web_resource_fixture_setup,
               test_webkit_web_resource_get_encoding, web_resource_fixture_teardown);
    g_test_add("/webkit/webresource/get_frame_name",
               WebResourceFixture, 0, web_resource_fixture_setup,
               test_webkit_web_resource_get_frame_name, web_resource_fixture_teardown);
    g_test_add("/webkit/webresource/get_data",
               WebResourceFixture, 0, web_resource_fixture_setup,
               test_webkit_web_resource_get_data, web_resource_fixture_teardown);
    return g_test_run ();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
