/*
 * Copyright (C) 2009, 2010 Gustavo Noronha Silva
 * Copyright (C) 2009, 2011 Igalia S.L.
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
#include <libsoup/soup.h>
#include <string.h>
#include <webkit2/webkit2.h>

/* This string has to be rather big because of the cancelled test - it
 * looks like soup refuses to send or receive a too small chunk */
#define HTML_STRING "<html><body>Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!</body></html>"

SoupURI *baseURI;

/* For real request testing */
static void serverCallback(SoupServer* server, SoupMessage* msg, const char* path, GHashTable* query, SoupClientContext* context, gpointer data)
{
    if (msg->method != SOUP_METHOD_GET) {
        soup_message_set_status(msg, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    soup_message_set_status(msg, SOUP_STATUS_OK);

    if (g_str_equal(path, "/")) {
        soup_message_set_status(msg, SOUP_STATUS_MOVED_PERMANENTLY);
        soup_message_headers_append(msg->response_headers, "Location", "/test_loading_status");
    } else if (g_str_equal(path, "/test_loading_status") || g_str_equal(path, "/test_loading_status2"))
        soup_message_body_append(msg->response_body, SOUP_MEMORY_STATIC, HTML_STRING, strlen(HTML_STRING));
    else if (g_str_equal(path, "/test_load_error")) {
        soup_message_set_status(msg, SOUP_STATUS_CANT_CONNECT);
    } else if (g_str_equal(path, "/test_loading_cancelled")) {
        soup_message_headers_set_encoding(msg->response_headers, SOUP_ENCODING_CHUNKED);
        soup_message_body_append(msg->response_body, SOUP_MEMORY_STATIC, HTML_STRING, strlen(HTML_STRING));
        soup_server_unpause_message(server, msg);
        return;
    }

    soup_message_body_complete(msg->response_body);
}

typedef struct {
    WebKitWebView *webView;
    GMainLoop *loop;
    gboolean hasBeenProvisional;
    gboolean hasBeenRedirect;
    gboolean hasBeenCommitted;
    gboolean hasBeenFinished;
    gboolean hasBeenFailed;
} WebLoadingFixture;

static void webLoadingFixtureSetup(WebLoadingFixture *fixture, gconstpointer data)
{
    fixture->webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    fixture->loop = g_main_loop_new(NULL, TRUE);
    g_object_ref_sink(fixture->webView);
    fixture->hasBeenProvisional = FALSE;
    fixture->hasBeenRedirect = FALSE;
    fixture->hasBeenCommitted = FALSE;
    fixture->hasBeenFinished = FALSE;
    fixture->hasBeenFailed = FALSE;
}

static void webLoadingFixtureTeardown(WebLoadingFixture *fixture, gconstpointer data)
{
    g_object_unref(fixture->webView);
    g_main_loop_unref(fixture->loop);
}

static char *getURIForPath(const char* path)
{
    SoupURI *uri;
    char *uriString;

    uri = soup_uri_new_with_base(baseURI, path);
    uriString = soup_uri_to_string(uri, FALSE);
    soup_uri_free(uri);

    return uriString;
}

/* Load Status */
static gboolean loadStatusProvisionalLoadStarted(WebKitWebLoaderClient *client, WebLoadingFixture *fixture)
{
    g_assert(!fixture->hasBeenProvisional);
    g_assert(!fixture->hasBeenRedirect);
    g_assert(!fixture->hasBeenCommitted);
    fixture->hasBeenProvisional = TRUE;

    return TRUE;
}

static gboolean loadStatusProvisionalLoadReceivedServerRedirect(WebKitWebLoaderClient *client, WebLoadingFixture *fixture)
{
    g_assert(fixture->hasBeenProvisional);
    g_assert(!fixture->hasBeenRedirect);
    g_assert(!fixture->hasBeenCommitted);
    fixture->hasBeenRedirect = TRUE;

    return TRUE;
}

static gboolean loadStatusProvisionalLoadFailed(WebKitWebLoaderClient *client, GError *error, WebLoadingFixture *fixture)
{
    g_assert_not_reached();
    return TRUE;
}

static gboolean loadStatusLoadCommitted(WebKitWebLoaderClient *client, WebLoadingFixture *fixture)
{
    g_assert(fixture->hasBeenProvisional);
    g_assert(fixture->hasBeenRedirect);
    g_assert(!fixture->hasBeenCommitted);
    fixture->hasBeenCommitted = TRUE;

    return TRUE;
}

static gboolean loadStatusLoadFinished(WebKitWebLoaderClient *client, WebLoadingFixture *fixture)
{
    g_assert(fixture->hasBeenProvisional);
    g_assert(fixture->hasBeenRedirect);
    g_assert(fixture->hasBeenCommitted);
    g_assert(!fixture->hasBeenFinished);
    fixture->hasBeenFinished = TRUE;

    g_main_loop_quit(fixture->loop);

    return TRUE;
}

static gboolean loadStatusLoadFailed(WebKitWebLoaderClient *client, GError *error, WebLoadingFixture *fixture)
{
    g_assert_not_reached();
    return TRUE;
}

static void testLoadingStatus(WebLoadingFixture *fixture, gconstpointer data)
{
    char *uriString;
    WebKitWebLoaderClient *client = webkit_web_view_get_loader_client(fixture->webView);

    g_signal_connect(client, "provisional-load-started", G_CALLBACK(loadStatusProvisionalLoadStarted), fixture);
    g_signal_connect(client, "provisional-load-received-server-redirect", G_CALLBACK(loadStatusProvisionalLoadReceivedServerRedirect), fixture);
    g_signal_connect(client, "provisional-load-failed", G_CALLBACK(loadStatusProvisionalLoadFailed), fixture);
    g_signal_connect(client, "load-committed", G_CALLBACK(loadStatusLoadCommitted), fixture);
    g_signal_connect(client, "load-finished", G_CALLBACK(loadStatusLoadFinished), fixture);
    g_signal_connect(client, "load-failed", G_CALLBACK(loadStatusLoadFailed), fixture);

    uriString = getURIForPath("/");
    webkit_web_view_load_uri(fixture->webView, uriString);
    g_free(uriString);

    g_main_loop_run(fixture->loop);

    g_assert(fixture->hasBeenProvisional);
    g_assert(fixture->hasBeenRedirect);
    g_assert(fixture->hasBeenCommitted);
    g_assert(fixture->hasBeenFinished);
}

/* Load Error */
static gboolean loadErrorProvisionalLoadStarted(WebKitWebLoaderClient *client, WebLoadingFixture *fixture)
{
    g_assert(!fixture->hasBeenProvisional);
    fixture->hasBeenProvisional = TRUE;

    return TRUE;
}

static gboolean loadErrorProvisionalLoadFailed(WebKitWebLoaderClient *client, GError *error, WebLoadingFixture *fixture)
{
    g_assert(fixture->hasBeenProvisional);
    g_assert(!fixture->hasBeenFailed);
    fixture->hasBeenFailed = TRUE;

    g_assert(error);

    g_main_loop_quit(fixture->loop);

    return TRUE;
}

static gboolean loadErrorLoadFinished(WebKitWebLoaderClient *client, WebLoadingFixture *fixture)
{
    g_assert_not_reached();
    return TRUE;
}

static void testLoadingError(WebLoadingFixture *fixture, gconstpointer data)
{
    char *uriString;
    WebKitWebLoaderClient *client = webkit_web_view_get_loader_client(fixture->webView);

    g_signal_connect(client, "provisional-load-started", G_CALLBACK(loadErrorProvisionalLoadStarted), fixture);
    g_signal_connect(client, "provisional-load-failed", G_CALLBACK(loadErrorProvisionalLoadFailed), fixture);
    g_signal_connect(client, "load-finished", G_CALLBACK(loadErrorLoadFinished), fixture);

    uriString = getURIForPath("/test_load_error");
    webkit_web_view_load_uri(fixture->webView, uriString);
    g_free(uriString);

    g_main_loop_run(fixture->loop);

    g_assert(fixture->hasBeenProvisional);
    g_assert(fixture->hasBeenFailed);
}

int main(int argc, char **argv)
{
    SoupServer *server;

    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_setenv("WEBKIT_EXEC_PATH", WEBKIT_EXEC_PATH, FALSE);

    server = soup_server_new(SOUP_SERVER_PORT, 0, NULL);
    soup_server_run_async(server);

    soup_server_add_handler(server, NULL, serverCallback, NULL, NULL);

    baseURI = soup_uri_new("http://127.0.0.1/");
    soup_uri_set_port(baseURI, soup_server_get_port(server));

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add("/webkit2/loading/status",
               WebLoadingFixture, NULL,
               webLoadingFixtureSetup,
               testLoadingStatus,
               webLoadingFixtureTeardown);
    g_test_add("/webkit2/loading/error",
               WebLoadingFixture, NULL,
               webLoadingFixtureSetup,
               testLoadingError,
               webLoadingFixtureTeardown);

    return g_test_run();
}

