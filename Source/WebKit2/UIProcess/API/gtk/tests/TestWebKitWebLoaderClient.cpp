/*
 * Copyright (C) 2009, 2010 Gustavo Noronha Silva
 * Copyright (C) 2009, 2011 Igalia S.L.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
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
#include "WebKitTestServer.h"
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <wtf/text/CString.h>

static WebKitTestServer* kServer;

static void testLoadingStatus(LoadTrackingTest* test, gconstpointer data)
{
    test->setRedirectURI(kServer->getURIForPath("/normal").data());
    test->loadURI(kServer->getURIForPath("/redirect").data());
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
    test->loadURI(kServer->getURIForPath("/error").data());
    test->waitUntilLoadFinished();

    Vector<LoadTrackingTest::LoadEvents>& events = test->m_loadEvents;
    g_assert_cmpint(events.size(), ==, 3);
    g_assert_cmpint(events[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(events[1], ==, LoadTrackingTest::ProvisionalLoadFailed);
    g_assert_cmpint(events[2], ==, LoadTrackingTest::LoadFinished);
}

static void assertNormalLoadHappened(Vector<LoadTrackingTest::LoadEvents>& events)
{
    g_assert_cmpint(events.size(), ==, 3);
    g_assert_cmpint(events[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(events[1], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(events[2], ==, LoadTrackingTest::LoadFinished);
}

static void testLoadHtml(LoadTrackingTest* test, gconstpointer)
{
    test->loadHtml("<html><body>Hello WebKit-GTK+</body></html>", 0);
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);
}

static void testLoadPlainText(LoadTrackingTest* test, gconstpointer)
{
    test->loadPlainText("Hello WebKit-GTK+");
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);
}

static void testLoadRequest(LoadTrackingTest* test, gconstpointer)
{
    GRefPtr<WebKitURIRequest> request(webkit_uri_request_new(kServer->getURIForPath("/normal").data()));
    test->loadRequest(request.get());
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);
}

class LoadStopTrackingTest : public LoadTrackingTest {
public:
    MAKE_GLIB_TEST_FIXTURE(LoadStopTrackingTest);

    virtual void loadCommitted()
    {
        LoadTrackingTest::loadCommitted();
        webkit_web_view_stop_loading(m_webView);
    }
    virtual void loadFailed(const gchar* failingURI, GError* error)
    {
        g_assert(g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED));
        LoadTrackingTest::loadFailed(failingURI, error);
    }
};

static void testLoadCancelled(LoadStopTrackingTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/cancelled").data());
    test->waitUntilLoadFinished();

    Vector<LoadTrackingTest::LoadEvents>& events = test->m_loadEvents;
    g_assert_cmpint(events.size(), ==, 4);
    g_assert_cmpint(events[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(events[1], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(events[2], ==, LoadTrackingTest::LoadFailed);
    g_assert_cmpint(events[3], ==, LoadTrackingTest::LoadFinished);
}

static void testWebViewTitle(LoadTrackingTest* test, gconstpointer)
{
    g_assert(!webkit_web_view_get_title(test->m_webView));
    test->loadHtml("<html><head><title>Welcome to WebKit-GTK+!</title></head></html>", 0);
    test->waitUntilLoadFinished();
    g_assert_cmpstr(webkit_web_view_get_title(test->m_webView), ==, "Welcome to WebKit-GTK+!");
}

static void testWebViewReload(LoadTrackingTest* test, gconstpointer)
{
    // Check that nothing happens when there's nothing to reload.
    webkit_web_view_reload(test->m_webView);
    test->wait(0.25); // Wait for a quarter of a second.

    test->loadURI(kServer->getURIForPath("/normal").data());
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);

    webkit_web_view_reload(test->m_webView);
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);
}

static void testLoadProgress(LoadTrackingTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/normal").data());
    test->waitUntilLoadFinished();
    g_assert_cmpfloat(test->m_estimatedProgress, ==, 1);
}

class ViewURITrackingTest: public LoadTrackingTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ViewURITrackingTest);

    static void uriChanged(GObject*, GParamSpec*, ViewURITrackingTest* test)
    {
        g_assert_cmpstr(test->m_activeURI.data(), !=, webkit_web_view_get_uri(test->m_webView));
        test->m_activeURI = webkit_web_view_get_uri(test->m_webView);
    }

    ViewURITrackingTest()
        : m_activeURI(webkit_web_view_get_uri(m_webView))
    {
        g_assert(m_activeURI.isNull());
        g_signal_connect(m_webView, "notify::uri", G_CALLBACK(uriChanged), this);
    }

    void provisionalLoadStarted()
    {
        checkActiveURI("/redirect");
    }

    void provisionalLoadReceivedServerRedirect()
    {
        checkActiveURI("/normal");
    }

    void loadCommitted()
    {
        checkActiveURI("/normal");
    }

    void loadFinished()
    {
        checkActiveURI("/normal");
        LoadTrackingTest::loadFinished();
    }

    CString m_activeURI;

private:
    void checkActiveURI(const char* uri)
    {
        // g_assert_cmpstr is a macro, so we need to cache the temporary string.
        CString serverURI = kServer->getURIForPath(uri);
        g_assert_cmpstr(m_activeURI.data(), ==, serverURI.data());
    }
};

static void testWebViewActiveURI(ViewURITrackingTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/redirect").data());
    test->waitUntilLoadFinished();
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
    } else if (g_str_equal(path, "/cancelled")) {
        soup_message_headers_set_encoding(message->response_headers, SOUP_ENCODING_CHUNKED);
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, responseString, strlen(responseString));
        soup_server_unpause_message(server, message);
        return;
    }

    soup_message_body_complete(message->response_body);
}

void beforeAll()
{
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

    LoadTrackingTest::add("WebKitWebLoaderClient", "loading-status", testLoadingStatus);
    LoadTrackingTest::add("WebKitWebLoaderClient", "loading-error", testLoadingError);
    LoadTrackingTest::add("WebKitWebView", "load-html", testLoadHtml);
    LoadTrackingTest::add("WebKitWebView", "load-plain-text", testLoadPlainText);
    LoadTrackingTest::add("WebKitWebView", "load-request", testLoadRequest);
    LoadStopTrackingTest::add("WebKitWebView", "stop-loading", testLoadCancelled);
    LoadTrackingTest::add("WebKitWebView", "title", testWebViewTitle);
    LoadTrackingTest::add("WebKitWebView", "progress", testLoadProgress);
    LoadTrackingTest::add("WebKitWebView", "reload", testWebViewReload);

    // This test checks that web view notify::uri signal is correctly emitted
    // and the uri is already updated when loader client signals are emitted.
    ViewURITrackingTest::add("WebKitWebView", "active-uri", testWebViewActiveURI);
}

void afterAll()
{
    delete kServer;
}
