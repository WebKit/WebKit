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

#include "config.h"

#include "LoadTrackingTest.h"
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <wtf/text/CString.h>

static SoupURI* kBaseURI = 0;
static SoupServer* kSoupServer = 0;

static CString getURIForPath(const char* path)
{
    SoupURI* uri = soup_uri_new_with_base(kBaseURI, path);
    GOwnPtr<gchar> uriString(soup_uri_to_string(uri, FALSE));
    soup_uri_free(uri);
    return uriString.get();
}

static void testLoadingStatus(LoadTrackingTest* test, gconstpointer data)
{
    webkit_web_view_load_uri(test->m_webView, getURIForPath("/redirect").data());
    test->waitUntilLoadFinished();

    Vector<LoadTrackingTest::LoadEvents>& events = test->m_loadEvents;
    g_assert_cmpint(events.size(), ==, 4);
    g_assert_cmpint(events[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(events[1], ==, LoadTrackingTest::ProvisionalLoadReceivedServerRedirect);
    g_assert_cmpint(events[2], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(events[3], ==, LoadTrackingTest::LoadFinished);
}

static void testLoadingError(LoadTrackingTest* test, gconstpointer)
{
    webkit_web_view_load_uri(test->m_webView, getURIForPath("/error").data());
    test->waitUntilLoadFinished();

    Vector<LoadTrackingTest::LoadEvents>& events = test->m_loadEvents;
    g_assert_cmpint(events.size(), ==, 2);
    g_assert_cmpint(events[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(events[1], ==, LoadTrackingTest::ProvisionalLoadFailed);
}

static void assertNormalLoadHappenedAndClearEvents(Vector<LoadTrackingTest::LoadEvents>& events)
{
    g_assert_cmpint(events.size(), ==, 3);
    g_assert_cmpint(events[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(events[1], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(events[2], ==, LoadTrackingTest::LoadFinished);
    events.clear();
}

static void testLoadAlternateContent(LoadTrackingTest* test, gconstpointer)
{
    webkit_web_view_load_alternate_html(test->m_webView,
                                        "<html><body>Alternate Content</body></html>",
                                        0, getURIForPath("/alternate").data());
    test->waitUntilLoadFinished();
    assertNormalLoadHappenedAndClearEvents(test->m_loadEvents);
}

static void testWebViewReload(LoadTrackingTest* test, gconstpointer)
{
    // Check that nothing happens when there's nothing to reload.
    webkit_web_view_reload(test->m_webView);
    test->wait(0.25); // Wait for a quarter of a second.

    webkit_web_view_load_uri(test->m_webView, getURIForPath("/normal").data());
    test->waitUntilLoadFinished();
    assertNormalLoadHappenedAndClearEvents(test->m_loadEvents);

    webkit_web_view_reload(test->m_webView);
    test->waitUntilLoadFinished();
    assertNormalLoadHappenedAndClearEvents(test->m_loadEvents);
}

static void testLoadProgress(LoadTrackingTest* test, gconstpointer)
{
    webkit_web_view_load_uri(test->m_webView, getURIForPath("/normal").data());
    test->waitUntilLoadFinished();
    g_assert_cmpfloat(test->m_estimatedProgress, ==, 1);
}

static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    static const char* responseString = "<html><body>Testing!Testing!Testing!Testing!Testing!Testing!Testing!"
        "Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!"
        "Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!"
        "Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!"
        "Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!"
        "Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!"
        "Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!"
        "Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!Testing!</body></html>";

    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    soup_message_set_status(message, SOUP_STATUS_OK);

    if (g_str_equal(path, "/normal"))
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, responseString, strlen(responseString));
    else if (g_str_equal(path, "/error"))
        soup_message_set_status(message, SOUP_STATUS_CANT_CONNECT);
    else if (g_str_equal(path, "/redirect")) {
        soup_message_set_status(message, SOUP_STATUS_MOVED_PERMANENTLY);
        soup_message_headers_append(message->response_headers, "Location", "/normal");
    }

    soup_message_body_complete(message->response_body);
}

void beforeAll()
{
    kBaseURI = soup_uri_new("http://127.0.0.1/");

    kSoupServer = soup_server_new(SOUP_SERVER_PORT, 0, NULL);
    soup_uri_set_port(kBaseURI, soup_server_get_port(kSoupServer));
    soup_server_run_async(kSoupServer);
    soup_server_add_handler(kSoupServer, 0, serverCallback, 0, 0);

    LoadTrackingTest::add("WebKitWebLoaderClient", "loading-status", testLoadingStatus);
    LoadTrackingTest::add("WebKitWebLoaderClient", "loading-error", testLoadingError);
    LoadTrackingTest::add("WebKitWebLoaderClient", "load-alternate-content", testLoadAlternateContent);
    LoadTrackingTest::add("WebKitWebView", "progress", testLoadProgress);
    LoadTrackingTest::add("WebKitWebView", "reload", testWebViewReload);
}

void afterAll()
{
}
