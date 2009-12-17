/*
 * Copyright (C) 2009 Jan Michael Alonzo
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

#include <glib.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <string.h>
#include <webkit/webkit.h>
#include <unistd.h>

#if GLIB_CHECK_VERSION(2, 16, 0) && GTK_CHECK_VERSION(2, 14, 0)

GMainLoop* loop;
SoupSession *session;
char* base_uri;

/* For real request testing */
static void
server_callback(SoupServer *server, SoupMessage *msg,
                 const char *path, GHashTable *query,
                 SoupClientContext *context, gpointer data)
{
    if (msg->method != SOUP_METHOD_GET) {
        soup_message_set_status(msg, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    soup_message_set_status(msg, SOUP_STATUS_OK);

    /* PDF */
    if (g_str_equal(path, "/pdf")) {
        char* contents;
        gsize length;
        GError* error = NULL;

        g_file_get_contents("test.pdf", &contents, &length, &error);
        g_assert(!error);

        soup_message_body_append(msg->response_body, SOUP_MEMORY_TAKE, contents, length);
    } else if (g_str_equal(path, "/html")) {
        char* contents;
        gsize length;
        GError* error = NULL;

        g_file_get_contents("test.html", &contents, &length, &error);
        g_assert(!error);

        soup_message_body_append(msg->response_body, SOUP_MEMORY_TAKE, contents, length);
    } else if (g_str_equal(path, "/text")) {
        char* contents;
        gsize length;
        GError* error = NULL;

        soup_message_headers_append(msg->response_headers, "Content-Disposition", "attachment; filename=test.txt");

        g_file_get_contents("test.txt", &contents, &length, &error);
        g_assert(!error);

        soup_message_body_append(msg->response_body, SOUP_MEMORY_TAKE, contents, length);
    } else if (g_str_equal(path, "/ogg")) {
        char* contents;
        gsize length;
        GError* error = NULL;

        g_file_get_contents("test.ogg", &contents, &length, &error);
        g_assert(!error);

        soup_message_body_append(msg->response_body, SOUP_MEMORY_TAKE, contents, length);
    }

    soup_message_body_complete(msg->response_body);
}

static void idle_quit_loop_cb(WebKitWebView* web_view, GParamSpec* pspec, gpointer data)
{
    if (webkit_web_view_get_load_status(web_view) == WEBKIT_LOAD_FINISHED ||
        webkit_web_view_get_load_status(web_view) == WEBKIT_LOAD_FAILED)
        g_main_loop_quit(loop);
}

static gboolean mime_type_policy_decision_requested_cb(WebKitWebView* view, WebKitWebFrame* frame,
                                                       WebKitNetworkRequest* request, const char* mime_type,
                                                       WebKitWebPolicyDecision* decision, gpointer data)
{
    char* type = (char*)data;

    if (g_str_equal(type, "pdf")) {
        g_assert_cmpstr(mime_type, ==, "application/pdf");
        g_assert(!webkit_web_view_can_show_mime_type(view, mime_type));
    } else if (g_str_equal(type, "html")) {
        g_assert_cmpstr(mime_type, ==, "text/html");
        g_assert(webkit_web_view_can_show_mime_type(view, mime_type));
    } else if (g_str_equal(type, "text")) {
        WebKitNetworkResponse* response = webkit_web_frame_get_network_response(frame);
        SoupMessage* message = webkit_network_response_get_message(response);
        char* disposition;

        g_assert(message);
        soup_message_headers_get_content_disposition(message->response_headers,
                                                     &disposition, NULL);
        g_object_unref(response);

        g_assert_cmpstr(disposition, ==, "attachment");
        g_free(disposition);

        g_assert_cmpstr(mime_type, ==, "text/plain");
        g_assert(webkit_web_view_can_show_mime_type(view, mime_type));
    } else if (g_str_equal(type, "ogg")) {
        g_assert_cmpstr(mime_type, ==, "audio/ogg");
        g_assert(webkit_web_view_can_show_mime_type(view, mime_type));
    }

    g_free(type);

    return FALSE;
}

static void test_mime_type(const char* name)
{
    WebKitWebView* view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(G_OBJECT(view));

    loop = g_main_loop_new(NULL, TRUE);

    g_object_connect(G_OBJECT(view),
                     "signal::notify::load-status", idle_quit_loop_cb, NULL,
                     "signal::mime-type-policy-decision-requested", mime_type_policy_decision_requested_cb, g_strdup(name),
                     NULL);

    char* effective_uri = g_strdup_printf("%s%s", base_uri, name);
    webkit_web_view_load_uri(view, effective_uri);
    g_free(effective_uri);

    g_main_loop_run(loop);

    g_object_unref(view);
}

static void test_mime_pdf()
{
    test_mime_type("pdf");
}

static void test_mime_html()
{
    test_mime_type("html");
}

static void test_mime_text()
{
    test_mime_type("text");
}

static void test_mime_ogg()
{
    test_mime_type("pdf");
}

int main(int argc, char** argv)
{
    SoupServer* server;
    SoupURI* soup_uri;

    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    /* Hopefully make test independent of the path it's called from. */
    while (!g_file_test ("WebKit/gtk/tests/resources/test.html", G_FILE_TEST_EXISTS)) {
        char path_name[PATH_MAX];

        g_chdir("..");

        g_assert(!g_str_equal(getcwd(path_name, PATH_MAX), "/"));
    }

    g_chdir("WebKit/gtk/tests/resources/");

    server = soup_server_new(SOUP_SERVER_PORT, 0, NULL);
    soup_server_run_async(server);

    soup_server_add_handler(server, NULL, server_callback, NULL, NULL);

    soup_uri = soup_uri_new("http://127.0.0.1/");
    soup_uri_set_port(soup_uri, soup_server_get_port(server));

    base_uri = soup_uri_to_string(soup_uri, FALSE);
    soup_uri_free(soup_uri);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add_func("/webkit/mime/PDF", test_mime_pdf);
    g_test_add_func("/webkit/mime/HTML", test_mime_html);
    g_test_add_func("/webkit/mime/TEXT", test_mime_text);
    g_test_add_func("/webkit/mime/OGG", test_mime_ogg);

    return g_test_run();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
