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
#include "WebKitTestBus.h"
#include "WebKitTestServer.h"
#include "WebViewTest.h"
#include <libsoup/soup.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

static WebKitTestBus* bus;
static WebKitTestServer* kServer;

const char* kDNTHeaderNotPresent = "DNT header not present";

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

static void testLoadAlternateHTML(LoadTrackingTest* test, gconstpointer)
{
    test->loadAlternateHTML("<html><body>Alternate page</body></html>", "http://error-page.foo/", 0);
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);
}

static void testLoadAlternateHTMLForLocalPage(LoadTrackingTest* test, gconstpointer)
{
    test->loadAlternateHTML("<html><body>Alternate page</body></html>", "file:///not/actually/loaded.html", 0);
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);
}

static void testLoadPlainText(LoadTrackingTest* test, gconstpointer)
{
    test->loadPlainText("Hello WebKit-GTK+");
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);
}

static void testLoadBytes(LoadTrackingTest* test, gconstpointer)
{
    GUniquePtr<char> filePath(g_build_filename(Test::getResourcesDir().data(), "blank.ico", nullptr));
    char* contents;
    gsize contentsLength;
    g_file_get_contents(filePath.get(), &contents, &contentsLength, nullptr);
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_take(contents, contentsLength));
    test->loadBytes(bytes.get(), "image/vnd.microsoft.icon", nullptr, nullptr);
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

static void testLoadFromGResource(LoadTrackingTest* test, gconstpointer)
{
    GRefPtr<WebKitURIRequest> request(webkit_uri_request_new("resource:///org/webkit/glib/tests/boring.html"));
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
        g_assert_error(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED);
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
    g_assert_null(webkit_web_view_get_title(test->m_webView));
    test->loadHtml("<html><head><title>Welcome to WebKit-GTK+!</title></head></html>", 0);
    test->waitUntilLoadFinished();
    g_assert_cmpstr(webkit_web_view_get_title(test->m_webView), ==, "Welcome to WebKit-GTK+!");
}

static void testWebViewReload(LoadTrackingTest* test, gconstpointer)
{
    // Check that nothing happens when there's nothing to reload.
    test->reload();
    test->wait(0.25); // Wait for a quarter of a second.

    test->loadURI(kServer->getURIForPath("/normal").data());
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);

    test->reload();
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);
}

static void testLoadProgress(LoadTrackingTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/normal").data());
    test->waitUntilLoadFinished();
    g_assert_cmpfloat(test->m_estimatedProgress, ==, 1);
}

static void testWebViewHistoryLoad(LoadTrackingTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/normal").data());
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);

    test->loadURI(kServer->getURIForPath("/normal2").data());
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);

    // Check that load process is the same for pages loaded from history cache.
    test->goBack();
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);

    test->goForward();
    test->waitUntilLoadFinished();
    assertNormalLoadHappened(test->m_loadEvents);
}

class LoadTwiceAndReloadTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(LoadTwiceAndReloadTest);

    static void reloadOnFinishLoad(WebKitWebView* view, WebKitLoadEvent loadEvent, LoadTwiceAndReloadTest* test)
    {
        if (++test->m_loadsCount == 3)
            test->quitMainLoop();
        webkit_web_view_reload(view);
    }

    LoadTwiceAndReloadTest()
    {
        g_signal_connect(m_webView, "load-changed", G_CALLBACK(reloadOnFinishLoad), this);
    }

    ~LoadTwiceAndReloadTest()
    {
        g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    }

    void waitUntilFinished()
    {
        m_loadsCount = 0;
        g_main_loop_run(m_mainLoop);
    }

    unsigned m_loadsCount { 0 };
};

static void testWebViewLoadTwiceAndReload(LoadTwiceAndReloadTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/normal").data());
    test->loadURI(kServer->getURIForPath("/normal2").data());
    test->waitUntilFinished();
}

static void uriChanged(WebKitWebView* webView, GParamSpec*, LoadTrackingTest* test)
{
    const char* uri = webkit_web_view_get_uri(webView);
    if (g_str_has_suffix(uri, "/normal"))
        test->m_activeURI = uri;
}

static void testUnfinishedSubresourceLoad(LoadTrackingTest* test, gconstpointer)
{
    // Verify that LoadFinished occurs even if the next load starts before the
    // previous load actually finishes.
    test->loadURI(kServer->getURIForPath("/unfinished-subresource-load").data());
    auto signalID = g_signal_connect(test->m_webView, "notify::uri", G_CALLBACK(uriChanged), test);
    test->waitUntilLoadFinished();
    test->waitUntilLoadFinished();
    g_signal_handler_disconnect(test->m_webView, signalID);

    Vector<LoadTrackingTest::LoadEvents>& events = test->m_loadEvents;
    g_assert_cmpint(events.size(), ==, 7);
    g_assert_cmpint(events[0], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(events[1], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(events[2], ==, LoadTrackingTest::LoadFailed);
    g_assert_cmpint(events[3], ==, LoadTrackingTest::LoadFinished);
    g_assert_cmpint(events[4], ==, LoadTrackingTest::ProvisionalLoadStarted);
    g_assert_cmpint(events[5], ==, LoadTrackingTest::LoadCommitted);
    g_assert_cmpint(events[6], ==, LoadTrackingTest::LoadFinished);
}

class ViewURITrackingTest: public LoadTrackingTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ViewURITrackingTest);

    static void uriChanged(GObject*, GParamSpec*, ViewURITrackingTest* test)
    {
        g_assert_cmpstr(test->m_currentURI.data(), !=, webkit_web_view_get_uri(test->m_webView));
        test->m_currentURI = webkit_web_view_get_uri(test->m_webView);
    }

    ViewURITrackingTest()
        : m_currentURI(webkit_web_view_get_uri(m_webView))
    {
        g_assert_true(m_currentURI.isNull());
        m_currentURIList.grow(m_currentURIList.capacity());
        g_signal_connect(m_webView, "notify::uri", G_CALLBACK(uriChanged), this);
    }

    enum State { Provisional, ProvisionalAfterRedirect, Commited, Finished };

    void loadURI(const char* uri)
    {
        reset();
        LoadTrackingTest::loadURI(uri);
    }

    void loadURIAndRedirectOnCommitted(const char* uri, const char* redirectURI)
    {
        reset();
        m_uriToLoadOnCommitted = redirectURI;
        LoadTrackingTest::loadURI(uri);
    }

    void provisionalLoadStarted()
    {
        m_currentURIList[Provisional] = m_currentURI;
    }

    void provisionalLoadReceivedServerRedirect()
    {
        m_currentURIList[ProvisionalAfterRedirect] = m_currentURI;
    }

    void loadCommitted()
    {
        m_currentURIList[Commited] = m_currentURI;
        if (!m_uriToLoadOnCommitted.isNull()) {
            m_estimatedProgress = 0;
            m_activeURI = m_uriToLoadOnCommitted;
            webkit_web_view_load_uri(m_webView, m_uriToLoadOnCommitted.data());
        }
    }

    void loadFinished()
    {
        m_currentURIList[Finished] = m_currentURI;
        LoadTrackingTest::loadFinished();
        if (!m_uriToLoadOnCommitted.isNull())
            m_uriToLoadOnCommitted = { };
    }

    void checkURIAtState(State state, const char* path)
    {
        if (path)
            ASSERT_CMP_CSTRING(m_currentURIList[state], ==, kServer->getURIForPath(path));
        else
            g_assert_true(m_currentURIList[state].isNull());
    }

private:
    void reset()
    {
        m_currentURI = { };
        m_uriToLoadOnCommitted = { };
        m_currentURIList.clear();
        m_currentURIList.grow(m_currentURIList.capacity());
    }

    CString m_currentURI;
    CString m_uriToLoadOnCommitted;
    Vector<CString, 4> m_currentURIList;
};

static void testWebViewActiveURI(ViewURITrackingTest* test, gconstpointer)
{
    // Normal load, the URL doesn't change.
    test->loadURI(kServer->getURIForPath("/normal1").data());
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/normal1");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, nullptr);
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/normal1");
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/normal1");

    // Redirect, the URL changes after the redirect.
    test->loadURI(kServer->getURIForPath("/redirect").data());
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/redirect");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, "/normal");
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/normal");
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/normal");

    // Normal load, URL changed by WebKitPage::send-request.
    test->loadURI(kServer->getURIForPath("/normal-change-request").data());
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/normal-change-request");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, nullptr);
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/request-changed");
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/request-changed");

    // Redirect, URL changed by WebKitPage::send-request.
    test->loadURI(kServer->getURIForPath("/redirect-to-change-request").data());
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/redirect-to-change-request");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, "/normal-change-request");
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/request-changed-on-redirect");
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/request-changed-on-redirect");

    // Non-API request loads.
    test->loadURI(kServer->getURIForPath("/redirect-js/normal").data());
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/redirect-js/normal");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, nullptr);
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/redirect-js/normal");
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/redirect-js/normal");
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/redirect-js/normal");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, nullptr);
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/normal");
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/normal");

    test->loadURI(kServer->getURIForPath("/redirect-js/redirect").data());
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/redirect-js/redirect");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, nullptr);
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/redirect-js/redirect");
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/redirect-js/redirect");
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/redirect-js/redirect");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, "/normal");
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/normal");
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/normal");

    test->loadURI(kServer->getURIForPath("/redirect-js/normal-change-request").data());
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/redirect-js/normal-change-request");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, nullptr);
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/redirect-js/normal-change-request");
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/redirect-js/normal-change-request");
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/redirect-js/normal-change-request");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, nullptr);
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/request-changed");
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/request-changed");

    test->loadURI(kServer->getURIForPath("/redirect-js/redirect-to-change-request").data());
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/redirect-js/redirect-to-change-request");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, nullptr);
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/redirect-js/redirect-to-change-request");
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/redirect-js/redirect-to-change-request");
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/redirect-js/redirect-to-change-request");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, "/normal-change-request");
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/request-changed-on-redirect");
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/request-changed-on-redirect");

    test->loadURIAndRedirectOnCommitted(kServer->getURIForPath("/normal").data(), kServer->getURIForPath("/headers").data());
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/normal");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, nullptr);
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/normal");
    // Pending API request is always updated immedately.
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/headers");
    test->waitUntilLoadFinished();
    test->checkURIAtState(ViewURITrackingTest::State::Provisional, "/headers");
    test->checkURIAtState(ViewURITrackingTest::State::ProvisionalAfterRedirect, nullptr);
    test->checkURIAtState(ViewURITrackingTest::State::Commited, "/headers");
    test->checkURIAtState(ViewURITrackingTest::State::Finished, "/headers");
}

class ViewIsLoadingTest: public LoadTrackingTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ViewIsLoadingTest);

    static void isLoadingChanged(GObject*, GParamSpec*, ViewIsLoadingTest* test)
    {
        if (webkit_web_view_is_loading(test->m_webView))
            test->beginLoad();
        else
            test->endLoad();
    }

    ViewIsLoadingTest()
    {
        g_signal_connect(m_webView, "notify::is-loading", G_CALLBACK(isLoadingChanged), this);
    }

    void beginLoad()
    {
        // New load, load-started hasn't been emitted yet.
        g_assert_true(m_loadEvents.isEmpty());
        g_assert_cmpstr(webkit_web_view_get_uri(m_webView), ==, m_activeURI.data());
    }

    void endLoad()
    {
        // Load finish, load-finished and load-failed haven't been emitted yet.
        g_assert_false(m_loadEvents.isEmpty());
        g_assert_false(m_loadEvents.contains(LoadTrackingTest::LoadFinished));
        g_assert_false(m_loadEvents.contains(LoadTrackingTest::LoadFailed));
    }
};

static void testWebViewIsLoading(ViewIsLoadingTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/normal").data());
    test->waitUntilLoadFinished();
    g_assert_false(webkit_web_view_is_loading(test->m_webView));

    test->reload();
    test->waitUntilLoadFinished();
    g_assert_false(webkit_web_view_is_loading(test->m_webView));

    test->loadURI(kServer->getURIForPath("/error").data());
    test->waitUntilLoadFinished();
    g_assert_false(webkit_web_view_is_loading(test->m_webView));

    test->loadURI(kServer->getURIForPath("/normal").data());
    test->waitUntilLoadFinished();
    g_assert_false(webkit_web_view_is_loading(test->m_webView));
    test->loadURI(kServer->getURIForPath("/normal2").data());
    test->waitUntilLoadFinished();
    g_assert_false(webkit_web_view_is_loading(test->m_webView));

    test->goBack();
    test->waitUntilLoadFinished();
    g_assert_false(webkit_web_view_is_loading(test->m_webView));

    test->goForward();
    test->waitUntilLoadFinished();
    g_assert_false(webkit_web_view_is_loading(test->m_webView));

    test->loadAlternateHTML("<html><head><title>Title</title></head></html>", "file:///foo", nullptr);
    test->waitUntilLoadFinished();
    g_assert_false(webkit_web_view_is_loading(test->m_webView));
}

class WebPageURITest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(WebPageURITest);

    static void webPageURIChangedCallback(GDBusConnection*, const char*, const char*, const char*, const char*, GVariant* result, WebPageURITest* test)
    {
        const char* uri;
        g_variant_get(result, "(&s)", &uri);
        test->m_webPageURIs.append(uri);
    }

    static void webViewURIChanged(GObject*, GParamSpec*, WebPageURITest* test)
    {
        test->m_webViewURIs.append(webkit_web_view_get_uri(test->m_webView));
    }

    WebPageURITest()
    {
        GUniquePtr<char> extensionBusName(g_strdup_printf("org.webkit.gtk.WebExtensionTest%u", Test::s_webExtensionID));
        GRefPtr<GDBusProxy> proxy = adoptGRef(bus->createProxy(extensionBusName.get(),
            "/org/webkit/gtk/WebExtensionTest", "org.webkit.gtk.WebExtensionTest", m_mainLoop));
        m_uriChangedSignalID = g_dbus_connection_signal_subscribe(
            g_dbus_proxy_get_connection(proxy.get()),
            0,
            "org.webkit.gtk.WebExtensionTest",
            "URIChanged",
            "/org/webkit/gtk/WebExtensionTest",
            0,
            G_DBUS_SIGNAL_FLAGS_NONE,
            reinterpret_cast<GDBusSignalCallback>(webPageURIChangedCallback),
            this,
            0);
        g_assert_cmpuint(m_uriChangedSignalID, !=, 0);

        g_signal_connect(m_webView, "notify::uri", G_CALLBACK(webViewURIChanged), this);
    }

    ~WebPageURITest()
    {
        g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
        g_dbus_connection_signal_unsubscribe(bus->connection(), m_uriChangedSignalID);
    }

    void loadURI(const char* uri)
    {
        m_webPageURIs.clear();
        m_webViewURIs.clear();
        WebViewTest::loadURI(uri);
    }

    void checkViewAndPageURIsMatch() const
    {
        g_assert_cmpint(m_webPageURIs.size(), ==, m_webViewURIs.size());
        for (size_t i = 0; i < m_webPageURIs.size(); ++i)
            ASSERT_CMP_CSTRING(m_webPageURIs[i], ==, m_webViewURIs[i]);
    }

    unsigned m_uriChangedSignalID;
    Vector<CString> m_webPageURIs;
    Vector<CString> m_webViewURIs;
};

static void testWebPageURI(WebPageURITest* test, gconstpointer)
{
    // Normal load.
    test->loadURI(kServer->getURIForPath("/normal1").data());
    test->waitUntilLoadFinished();
    test->checkViewAndPageURIsMatch();
    g_assert_cmpint(test->m_webPageURIs.size(), ==, 1);
    ASSERT_CMP_CSTRING(test->m_webPageURIs[0], ==, kServer->getURIForPath("/normal1"));

    // Redirect
    test->loadURI(kServer->getURIForPath("/redirect").data());
    test->waitUntilLoadFinished();
    test->checkViewAndPageURIsMatch();
    g_assert_cmpint(test->m_webPageURIs.size(), ==, 2);
    ASSERT_CMP_CSTRING(test->m_webPageURIs[0], ==, kServer->getURIForPath("/redirect"));
    ASSERT_CMP_CSTRING(test->m_webPageURIs[1], ==, kServer->getURIForPath("/normal"));

    // Normal load, URL changed by WebKitPage::send-request.
    test->loadURI(kServer->getURIForPath("/normal-change-request").data());
    test->waitUntilLoadFinished();
    test->checkViewAndPageURIsMatch();
    g_assert_cmpint(test->m_webPageURIs.size(), ==, 2);
    ASSERT_CMP_CSTRING(test->m_webPageURIs[0], ==, kServer->getURIForPath("/normal-change-request"));
    ASSERT_CMP_CSTRING(test->m_webPageURIs[1], ==, kServer->getURIForPath("/request-changed"));

    // Redirect, URL changed by WebKitPage::send-request.
    test->loadURI(kServer->getURIForPath("/redirect-to-change-request").data());
    test->waitUntilLoadFinished();
    test->checkViewAndPageURIsMatch();
    g_assert_cmpint(test->m_webPageURIs.size(), ==, 3);
    ASSERT_CMP_CSTRING(test->m_webPageURIs[0], ==, kServer->getURIForPath("/redirect-to-change-request"));
    ASSERT_CMP_CSTRING(test->m_webPageURIs[1], ==, kServer->getURIForPath("/normal-change-request"));
    ASSERT_CMP_CSTRING(test->m_webPageURIs[2], ==, kServer->getURIForPath("/request-changed-on-redirect"));
}

static void testURIRequestHTTPHeaders(WebViewTest* test, gconstpointer)
{
    GRefPtr<WebKitURIRequest> uriRequest = adoptGRef(webkit_uri_request_new("file:///foo/bar"));
    g_assert_nonnull(uriRequest.get());
    g_assert_cmpstr(webkit_uri_request_get_uri(uriRequest.get()), ==, "file:///foo/bar");
    g_assert_null(webkit_uri_request_get_http_headers(uriRequest.get()));

    // Load a request with no Do Not Track header.
    webkit_uri_request_set_uri(uriRequest.get(), kServer->getURIForPath("/do-not-track-header").data());
    test->loadRequest(uriRequest.get());
    test->waitUntilLoadFinished();

    size_t mainResourceDataSize = 0;
    const char* mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpint(mainResourceDataSize, ==, strlen(kDNTHeaderNotPresent));
    g_assert_cmpint(strncmp(mainResourceData, kDNTHeaderNotPresent, mainResourceDataSize), ==, 0);

    // Add the Do Not Track header and load the request again.
    SoupMessageHeaders* headers = webkit_uri_request_get_http_headers(uriRequest.get());
    g_assert_nonnull(headers);
    soup_message_headers_append(headers, "DNT", "1");
    test->loadRequest(uriRequest.get());
    test->waitUntilLoadFinished();

    mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpint(mainResourceDataSize, ==, 1);
    g_assert_cmpint(strncmp(mainResourceData, "1", mainResourceDataSize), ==, 0);

    // Load a URI for which the web extension will add the Do Not Track header.
    test->loadURI(kServer->getURIForPath("/add-do-not-track-header").data());
    test->waitUntilLoadFinished();

    mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpint(mainResourceDataSize, ==, 1);
    g_assert_cmpint(strncmp(mainResourceData, "1", mainResourceDataSize), ==, 0);
}

static void testURIRequestHTTPMethod(WebViewTest* test, gconstpointer)
{
    GRefPtr<WebKitURIRequest> uriRequest = adoptGRef(webkit_uri_request_new("file:///foo/bar"));
    g_assert_nonnull(uriRequest.get());
    g_assert_cmpstr(webkit_uri_request_get_uri(uriRequest.get()), ==, "file:///foo/bar");
    g_assert_null(webkit_uri_request_get_http_method(uriRequest.get()));

    webkit_uri_request_set_uri(uriRequest.get(), kServer->getURIForPath("/http-get-method").data());
    test->loadRequest(uriRequest.get());
    test->waitUntilLoadFinished();

    test->runJavaScriptAndWaitUntilFinished("xhr = new XMLHttpRequest; xhr.open('POST', '/http-post-method', false); xhr.send();", nullptr);
}

static void testURIResponseHTTPHeaders(WebViewTest* test, gconstpointer)
{
    test->loadHtml("<html><body>No HTTP headers</body></html>", "file:///");
    test->waitUntilLoadFinished();
    WebKitWebResource* resource = webkit_web_view_get_main_resource(test->m_webView);
    g_assert_true(WEBKIT_IS_WEB_RESOURCE(resource));
    WebKitURIResponse* response = webkit_web_resource_get_response(resource);
    g_assert_true(WEBKIT_IS_URI_RESPONSE(response));
    g_assert_null(webkit_uri_response_get_http_headers(response));

    test->loadURI(kServer->getURIForPath("/headers").data());
    test->waitUntilLoadFinished();
    resource = webkit_web_view_get_main_resource(test->m_webView);
    g_assert_true(WEBKIT_IS_WEB_RESOURCE(resource));
    response = webkit_web_resource_get_response(resource);
    g_assert_true(WEBKIT_IS_URI_RESPONSE(response));
    SoupMessageHeaders* headers = webkit_uri_response_get_http_headers(response);
    g_assert_nonnull(headers);
    g_assert_cmpstr(soup_message_headers_get_one(headers, "Foo"), ==, "bar");
}

static void testRedirectToDataURI(WebViewTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/redirect-to-data").data());
    test->waitUntilLoadFinished();

    static const char* expectedData = "data-uri";
    size_t mainResourceDataSize = 0;
    const char* mainResourceData = test->mainResourceData(mainResourceDataSize);
    g_assert_cmpint(mainResourceDataSize, ==, strlen(expectedData));
    g_assert_cmpint(strncmp(mainResourceData, expectedData, mainResourceDataSize), ==, 0);
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

    if (g_str_has_prefix(path, "/normal") || g_str_has_prefix(path, "/http-get-method"))
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, responseString, strlen(responseString));
    else if (g_str_equal(path, "/error"))
        soup_message_set_status(message, SOUP_STATUS_CANT_CONNECT);
    else if (g_str_equal(path, "/redirect")) {
        soup_message_set_status(message, SOUP_STATUS_MOVED_PERMANENTLY);
        soup_message_headers_append(message->response_headers, "Location", "/normal");
    } else if (g_str_equal(path, "/redirect-to-change-request")) {
        soup_message_set_status(message, SOUP_STATUS_MOVED_PERMANENTLY);
        soup_message_headers_append(message->response_headers, "Location", "/normal-change-request");
    } else if (g_str_has_prefix(path, "/redirect-js/")) {
        static const char* redirectJSFormat = "<html><body><script>location = '%s';</script></body></html>";
        char* redirectJS = g_strdup_printf(redirectJSFormat, g_strrstr(path, "/"));
        soup_message_body_append(message->response_body, SOUP_MEMORY_TAKE, redirectJS, strlen(redirectJS));
    } else if (g_str_equal(path, "/cancelled")) {
        soup_message_headers_set_encoding(message->response_headers, SOUP_ENCODING_CHUNKED);
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, responseString, strlen(responseString));
        soup_server_unpause_message(server, message);
        return;
    } else if (g_str_equal(path, "/do-not-track-header") || g_str_equal(path, "/add-do-not-track-header")) {
        const char* doNotTrack = soup_message_headers_get_one(message->request_headers, "DNT");
        if (doNotTrack)
            soup_message_body_append(message->response_body, SOUP_MEMORY_COPY, doNotTrack, strlen(doNotTrack));
        else
            soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, kDNTHeaderNotPresent, strlen(kDNTHeaderNotPresent));
        soup_message_set_status(message, SOUP_STATUS_OK);
    } else if (g_str_equal(path, "/headers")) {
        soup_message_headers_append(message->response_headers, "Foo", "bar");
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, responseString, strlen(responseString));
    } else if (g_str_equal(path, "/redirect-to-data")) {
        soup_message_set_status(message, SOUP_STATUS_MOVED_PERMANENTLY);
        soup_message_headers_append(message->response_headers, "Location", "data:text/plain;charset=utf-8,data-uri");
    } else if (g_str_equal(path, "/unfinished-subresource-load")) {
        static const char* unfinishedSubresourceLoadResponseString = "<html><body>"
            "<img src=\"/stall\"/>"
            "<script>"
            "  function run() {"
            "      location = '/normal';"
            "  }"
            "  setInterval(run(), 50);"
            "</script>"
            "</body></html>";
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, unfinishedSubresourceLoadResponseString, strlen(unfinishedSubresourceLoadResponseString));
    } else if (g_str_equal(path, "/stall")) {
        // This request is never unpaused and stalls forever.
        soup_server_pause_message(server, message);
        return;
    } else
        soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);

    soup_message_body_complete(message->response_body);
}

void beforeAll()
{
    bus = new WebKitTestBus();
    if (!bus->run())
        return;

    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

    LoadTrackingTest::add("WebKitWebView", "loading-status", testLoadingStatus);
    LoadTrackingTest::add("WebKitWebView", "loading-error", testLoadingError);
    LoadTrackingTest::add("WebKitWebView", "load-html", testLoadHtml);
    LoadTrackingTest::add("WebKitWebView", "load-alternate-html", testLoadAlternateHTML);
    LoadTrackingTest::add("WebKitWebView", "load-alternate-html-for-local-page", testLoadAlternateHTMLForLocalPage);
    LoadTrackingTest::add("WebKitWebView", "load-plain-text", testLoadPlainText);
    LoadTrackingTest::add("WebKitWebView", "load-bytes", testLoadBytes);
    LoadTrackingTest::add("WebKitWebView", "load-request", testLoadRequest);
    LoadTrackingTest::add("WebKitWebView", "load-gresource", testLoadFromGResource);
    LoadStopTrackingTest::add("WebKitWebView", "stop-loading", testLoadCancelled);
    LoadTrackingTest::add("WebKitWebView", "title", testWebViewTitle);
    LoadTrackingTest::add("WebKitWebView", "progress", testLoadProgress);
    LoadTrackingTest::add("WebKitWebView", "reload", testWebViewReload);
    LoadTrackingTest::add("WebKitWebView", "history-load", testWebViewHistoryLoad);
    LoadTwiceAndReloadTest::add("WebKitWebView", "load-twice-and-reload", testWebViewLoadTwiceAndReload);
    LoadTrackingTest::add("WebKitWebView", "unfinished-subresource-load", testUnfinishedSubresourceLoad);

    // This test checks that web view notify::uri signal is correctly emitted
    // and the uri is already updated when loader client signals are emitted.
    ViewURITrackingTest::add("WebKitWebView", "active-uri", testWebViewActiveURI);

    ViewIsLoadingTest::add("WebKitWebView", "is-loading", testWebViewIsLoading);
    WebPageURITest::add("WebKitWebPage", "get-uri", testWebPageURI);
    WebViewTest::add("WebKitURIRequest", "http-headers", testURIRequestHTTPHeaders);
    WebViewTest::add("WebKitURIRequest", "http-method", testURIRequestHTTPMethod);
    WebViewTest::add("WebKitURIResponse", "http-headers", testURIResponseHTTPHeaders);
    WebViewTest::add("WebKitWebPage", "redirect-to-data-uri", testRedirectToDataURI);
}

void afterAll()
{
    delete bus;
    delete kServer;
}
