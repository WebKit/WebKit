/*
 * Copyright (C) 2012 Igalia S.L.
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

#include "WebKitTestServer.h"
#include "WebViewTest.h"
#include <WebCore/SoupVersioning.h>
#include <wtf/Vector.h>
#include <wtf/glib/GMutexLocker.h>
#include <wtf/glib/GRefPtr.h>

static WebKitTestServer* kServer;

static const char* kIndexHtml =
    "<html><head>"
    " <link rel='stylesheet' href='/style.css' type='text/css'>"
    " <script language='javascript' src='/javascript.js'></script>"
    "</head><body>WebKitGTK resources test</body></html>";

static const char* kStyleCSS =
    "body {"
    "    margin: 0px;"
    "    padding: 0px;"
    "    font-family: sans-serif;"
    "    background: url(/blank.ico) 0 0 no-repeat;"
    "    color: black;"
    "}";

static const char* kJavascript = "function foo () { var a = 1; }";

class ResourcesTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ResourcesTest);

    static void resourceSentRequestCallback(WebKitWebResource* resource, WebKitURIRequest* request, WebKitURIResponse* redirectResponse, ResourcesTest* test)
    {
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(request));
        if (redirectResponse)
            test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(redirectResponse));
        test->resourceSentRequest(resource, request, redirectResponse);
    }

    static void resourceReceivedResponseCallback(WebKitWebResource* resource, GParamSpec*, ResourcesTest* test)
    {
        g_assert_nonnull(webkit_web_resource_get_response(resource));
        test->resourceReceivedResponse(resource);
    }

    static void resourceReceivedDataCallback(WebKitWebResource* resource, guint64 bytesReceived, ResourcesTest* test)
    {
        test->resourceReceivedData(resource, bytesReceived);
    }

    static void resourceFinishedCallback(WebKitWebResource* resource, ResourcesTest* test)
    {
        test->resourceFinished(resource);
    }

    static void resourceFailedCallback(WebKitWebResource* resource, GError* error, ResourcesTest* test)
    {
        g_assert_nonnull(error);
        test->resourceFailed(resource, error);
    }

    static void resourceLoadStartedCallback(WebKitWebView* webView, WebKitWebResource* resource, WebKitURIRequest* request, ResourcesTest* test)
    {
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(resource));
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(request));

        // Ignore favicons.
        if (g_str_has_suffix(webkit_uri_request_get_uri(request), "favicon.ico"))
            return;

        test->resourceLoadStarted(resource, request);
        g_signal_connect(resource, "sent-request", G_CALLBACK(resourceSentRequestCallback), test);
        g_signal_connect(resource, "notify::response", G_CALLBACK(resourceReceivedResponseCallback), test);
        g_signal_connect(resource, "received-data", G_CALLBACK(resourceReceivedDataCallback), test);
        g_signal_connect(resource, "finished", G_CALLBACK(resourceFinishedCallback), test);
        g_signal_connect(resource, "failed", G_CALLBACK(resourceFailedCallback), test);
    }

    void clearSubresources()
    {
        g_list_free_full(m_subresources, reinterpret_cast<GDestroyNotify>(g_object_unref));
        m_subresources = 0;
    }

    ResourcesTest()
        : WebViewTest()
        , m_resourcesLoaded(0)
        , m_resourcesToLoad(0)
        , m_resourceDataSize(0)
        , m_subresources(0)
    {
        g_signal_connect(m_webView, "resource-load-started", G_CALLBACK(resourceLoadStartedCallback), this);
    }

    ~ResourcesTest()
    {
        clearSubresources();
    }

    virtual void resourceLoadStarted(WebKitWebResource* resource, WebKitURIRequest* request)
    {
    }

    virtual void resourceSentRequest(WebKitWebResource* resource, WebKitURIRequest* request, WebKitURIResponse* redirectResponse)
    {
    }

    virtual void resourceReceivedResponse(WebKitWebResource* resource)
    {
    }

    virtual void resourceReceivedData(WebKitWebResource* resource, guint64 bytesReceived)
    {
    }

    virtual void resourceFinished(WebKitWebResource* resource)
    {
        g_signal_handlers_disconnect_matched(resource, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
        if (webkit_web_view_get_main_resource(m_webView) != resource)
            m_subresources = g_list_prepend(m_subresources, g_object_ref(resource));
        if (++m_resourcesLoaded == m_resourcesToLoad)
            g_main_loop_quit(m_mainLoop);
    }

    virtual void resourceFailed(WebKitWebResource* resource, GError* error)
    {
        g_assert_not_reached();
    }

    void waitUntilResourcesLoaded(size_t resourcesCount)
    {
        m_resourcesLoaded = 0;
        m_resourcesToLoad = resourcesCount;
        clearSubresources();
        g_main_loop_run(m_mainLoop);
    }

    GList* subresources()
    {
        return m_subresources;
    }

    static void resourceGetDataCallback(GObject* object, GAsyncResult* result, gpointer userData)
    {
        size_t dataSize;
        GUniqueOutPtr<GError> error;
        unsigned char* data = webkit_web_resource_get_data_finish(WEBKIT_WEB_RESOURCE(object), result, &dataSize, &error.outPtr());
        g_assert_no_error(error.get());
        g_assert_nonnull(data);
        g_assert_cmpint(dataSize, >, 0);

        ResourcesTest* test = static_cast<ResourcesTest*>(userData);
        test->m_resourceData.reset(reinterpret_cast<char*>(data));
        test->m_resourceDataSize = dataSize;
        g_main_loop_quit(test->m_mainLoop);
    }

    void checkResourceData(WebKitWebResource* resource)
    {
        m_resourceDataSize = 0;
        webkit_web_resource_get_data(resource, 0, resourceGetDataCallback, this);
        g_main_loop_run(m_mainLoop);

        const char* uri = webkit_web_resource_get_uri(resource);
        if (uri == kServer->getURIForPath("/")) {
            g_assert_cmpint(m_resourceDataSize, ==, strlen(kIndexHtml));
            g_assert_cmpint(strncmp(m_resourceData.get(), kIndexHtml, m_resourceDataSize), ==, 0);
        } else if (uri == kServer->getURIForPath("/style.css")) {
            g_assert_cmpint(m_resourceDataSize, ==, strlen(kStyleCSS));
            g_assert_cmpint(strncmp(m_resourceData.get(), kStyleCSS, m_resourceDataSize), ==, 0);
        } else if (uri == kServer->getURIForPath("/javascript.js")) {
            g_assert_cmpint(m_resourceDataSize, ==, strlen(kJavascript));
            g_assert_cmpint(strncmp(m_resourceData.get(), kJavascript, m_resourceDataSize), ==, 0);
        } else if (uri == kServer->getURIForPath("/blank.ico")) {
            GUniquePtr<char> filePath(g_build_filename(Test::getResourcesDir().data(), "blank.ico", nullptr));
            GUniqueOutPtr<char> contents;
            gsize contentsLength;
            g_file_get_contents(filePath.get(), &contents.outPtr(), &contentsLength, nullptr);
            g_assert_cmpint(m_resourceDataSize, ==, contentsLength);
            g_assert_cmpmem(m_resourceData.get(), contentsLength, contents.get(), contentsLength);
        } else
            g_assert_not_reached();
        m_resourceData.reset();
    }

    size_t m_resourcesLoaded;
    size_t m_resourcesToLoad;
    GUniquePtr<char> m_resourceData;
    size_t m_resourceDataSize;
    GList* m_subresources;
};

static void testWebViewResources(ResourcesTest* test, gconstpointer)
{
    // Nothing loaded yet, there shoulnd't be resources.
    g_assert_null(webkit_web_view_get_main_resource(test->m_webView));
    g_assert_null(test->subresources());

    // Load simple page without subresources.
    test->loadHtml("<html><body>Testing WebKitGTK</body></html>", 0);
    test->waitUntilLoadFinished();
    WebKitWebResource* resource = webkit_web_view_get_main_resource(test->m_webView);
    g_assert_nonnull(resource);
    g_assert_cmpstr(webkit_web_view_get_uri(test->m_webView), ==, webkit_web_resource_get_uri(resource));
    g_assert_null(test->subresources());

    // Load simple page with subresources.
    test->loadURI(kServer->getURIForPath("/").data());
    test->waitUntilResourcesLoaded(4);

    resource = webkit_web_view_get_main_resource(test->m_webView);
    g_assert_nonnull(resource);
    g_assert_cmpstr(webkit_web_view_get_uri(test->m_webView), ==, webkit_web_resource_get_uri(resource));
    GList* subresources = test->subresources();
    g_assert_nonnull(subresources);
    g_assert_cmpint(g_list_length(subresources), ==, 3);

#if 0
    // Load the same URI again.
    // FIXME: we need a workaround for bug https://bugs.webkit.org/show_bug.cgi?id=78510.
    test->loadURI(kServer->getURIForPath("/").data());
    test->waitUntilResourcesLoaded(4);
#endif

    // Reload.
    webkit_web_view_reload_bypass_cache(test->m_webView);
    test->waitUntilResourcesLoaded(4);
}

class SingleResourceLoadTest: public ResourcesTest {
public:
    MAKE_GLIB_TEST_FIXTURE(SingleResourceLoadTest);

    enum LoadEvents {
        Started,
        SentRequest,
        Redirected,
        ReceivedResponse,
        ReceivedData,
        Finished,
        Failed
    };

    SingleResourceLoadTest()
        : ResourcesTest()
        , m_resourceDataReceived(0)
    {
        m_resourcesToLoad = 2;
    }

    void resourceLoadStarted(WebKitWebResource* resource, WebKitURIRequest* request)
    {
        if (resource == webkit_web_view_get_main_resource(m_webView))
            return;

        m_resourceDataReceived = 0;
        m_resource = resource;
        m_loadEvents.append(Started);
    }

    void resourceSentRequest(WebKitWebResource* resource, WebKitURIRequest* request, WebKitURIResponse* redirectResponse)
    {
        if (resource != m_resource)
            return;

        if (redirectResponse)
            m_loadEvents.append(Redirected);
        else
            m_loadEvents.append(SentRequest);
    }

    void resourceReceivedResponse(WebKitWebResource* resource)
    {
        if (resource != m_resource)
            return;

        m_loadEvents.append(ReceivedResponse);
    }

    void resourceReceivedData(WebKitWebResource* resource, guint64 bytesReceived)
    {
        if (resource != m_resource)
            return;

        m_resourceDataReceived += bytesReceived;
        if (!m_loadEvents.contains(ReceivedData))
            m_loadEvents.append(ReceivedData);
    }

    void resourceFinished(WebKitWebResource* resource)
    {
        if (resource != m_resource) {
            ResourcesTest::resourceFinished(resource);
            return;
        }

        if (!m_loadEvents.contains(Failed)) {
            WebKitURIResponse* response = webkit_web_resource_get_response(m_resource.get());
            g_assert_nonnull(response);
            g_assert_cmpint(webkit_uri_response_get_content_length(response), ==, m_resourceDataReceived);
        }
        m_loadEvents.append(Finished);
        ResourcesTest::resourceFinished(resource);
    }

    void resourceFailed(WebKitWebResource* resource, GError* error)
    {
        if (resource == m_resource)
            m_loadEvents.append(Failed);
    }

    void waitUntilResourceLoadFinished()
    {
        m_resource = 0;
        m_resourcesLoaded = 0;
        g_main_loop_run(m_mainLoop);
    }

    WebKitURIResponse* waitUntilResourceLoadFinishedAndReturnURIResponse()
    {
        waitUntilResourceLoadFinished();
        g_assert_nonnull(m_resource);
        return webkit_web_resource_get_response(m_resource.get());
    }

    GRefPtr<WebKitWebResource> m_resource;
    Vector<LoadEvents> m_loadEvents;
    guint64 m_resourceDataReceived;
};

static void testWebResourceLoading(SingleResourceLoadTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/javascript.html").data());
    test->waitUntilResourceLoadFinished();
    g_assert_nonnull(test->m_resource);
    Vector<SingleResourceLoadTest::LoadEvents>& events = test->m_loadEvents;
    g_assert_cmpint(events.size(), ==, 5);
    g_assert_cmpint(events[0], ==, SingleResourceLoadTest::Started);
    g_assert_cmpint(events[1], ==, SingleResourceLoadTest::SentRequest);
    g_assert_cmpint(events[2], ==, SingleResourceLoadTest::ReceivedResponse);
    g_assert_cmpint(events[3], ==, SingleResourceLoadTest::ReceivedData);
    g_assert_cmpint(events[4], ==, SingleResourceLoadTest::Finished);
    events.clear();

    test->loadURI(kServer->getURIForPath("/redirected-css.html").data());
    test->waitUntilResourceLoadFinished();
    g_assert_nonnull(test->m_resource);
    g_assert_cmpint(events.size(), ==, 6);
    g_assert_cmpint(events[0], ==, SingleResourceLoadTest::Started);
    g_assert_cmpint(events[1], ==, SingleResourceLoadTest::SentRequest);
    g_assert_cmpint(events[2], ==, SingleResourceLoadTest::Redirected);
    g_assert_cmpint(events[3], ==, SingleResourceLoadTest::ReceivedResponse);
    g_assert_cmpint(events[4], ==, SingleResourceLoadTest::ReceivedData);
    g_assert_cmpint(events[5], ==, SingleResourceLoadTest::Finished);
    events.clear();

    test->loadURI(kServer->getURIForPath("/invalid-css.html").data());
    test->waitUntilResourceLoadFinished();
    g_assert_nonnull(test->m_resource);
    g_assert_cmpint(events.size(), ==, 4);
    g_assert_cmpint(events[0], ==, SingleResourceLoadTest::Started);
    g_assert_cmpint(events[1], ==, SingleResourceLoadTest::SentRequest);
    g_assert_cmpint(events[2], ==, SingleResourceLoadTest::Failed);
    g_assert_cmpint(events[3], ==, SingleResourceLoadTest::Finished);
    events.clear();
}

static void testWebResourceResponse(SingleResourceLoadTest* test, gconstpointer)
{
    // No cached resource: First load.
    test->loadURI(kServer->getURIForPath("/javascript.html").data());
    WebKitURIResponse* response = test->waitUntilResourceLoadFinishedAndReturnURIResponse();
    g_assert_cmpint(webkit_uri_response_get_status_code(response), ==, SOUP_STATUS_OK);

    // No cached resource: Second load.
    test->loadURI(kServer->getURIForPath("/javascript.html").data());
    response = test->waitUntilResourceLoadFinishedAndReturnURIResponse();
    g_assert_cmpint(webkit_uri_response_get_status_code(response), ==, SOUP_STATUS_OK);

    // No cached resource: Reload.
    webkit_web_view_reload(test->m_webView);
    response = test->waitUntilResourceLoadFinishedAndReturnURIResponse();
    g_assert_cmpint(webkit_uri_response_get_status_code(response), ==, SOUP_STATUS_OK);

    // Cached resource: First load.
    test->loadURI(kServer->getURIForPath("/image.html").data());
    response = test->waitUntilResourceLoadFinishedAndReturnURIResponse();
    g_assert_cmpint(webkit_uri_response_get_status_code(response), ==, SOUP_STATUS_OK);

    // Cached resource: Second load.
    test->loadURI(kServer->getURIForPath("/image.html").data());
    response = test->waitUntilResourceLoadFinishedAndReturnURIResponse();
    g_assert_cmpint(webkit_uri_response_get_status_code(response), ==, SOUP_STATUS_OK);

    // Cached resource: Reload.
    webkit_web_view_reload(test->m_webView);
    response = test->waitUntilResourceLoadFinishedAndReturnURIResponse();
    g_assert_cmpint(webkit_uri_response_get_status_code(response), ==, SOUP_STATUS_NOT_MODIFIED);
}

static void testWebResourceMimeType(SingleResourceLoadTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/javascript.html").data());
    WebKitURIResponse* response = test->waitUntilResourceLoadFinishedAndReturnURIResponse();
    g_assert_cmpstr(webkit_uri_response_get_mime_type(response), ==, "text/javascript");

    test->loadURI(kServer->getURIForPath("/image.html").data());
    response = test->waitUntilResourceLoadFinishedAndReturnURIResponse();
    g_assert_cmpstr(webkit_uri_response_get_mime_type(response), ==, "image/x-icon");

    test->loadURI(kServer->getURIForPath("/redirected-css.html").data());
    response = test->waitUntilResourceLoadFinishedAndReturnURIResponse();
    g_assert_cmpstr(webkit_uri_response_get_mime_type(response), ==, "text/css");

    test->loadURI(kServer->getURIForPath("/iframe-no-content.html").data());
    response = test->waitUntilResourceLoadFinishedAndReturnURIResponse();
    g_assert_cmpstr(webkit_uri_response_get_mime_type(response), ==, "text/plain");
}

static void testWebResourceSuggestedFilename(SingleResourceLoadTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/javascript.html").data());
    WebKitURIResponse* response = test->waitUntilResourceLoadFinishedAndReturnURIResponse();
    g_assert_cmpstr(webkit_uri_response_get_suggested_filename(response), ==, "JavaScript.js");

    test->loadURI(kServer->getURIForPath("/image.html").data());
    response = test->waitUntilResourceLoadFinishedAndReturnURIResponse();
    g_assert_null(webkit_uri_response_get_suggested_filename(response));
}

class ResourceURITrackingTest: public SingleResourceLoadTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ResourceURITrackingTest);

    ResourceURITrackingTest()
        : SingleResourceLoadTest()
    {
    }

    static void uriChanged(WebKitWebResource* resource, GParamSpec*, ResourceURITrackingTest* test)
    {
        g_assert_true(resource == test->m_resource.get());
        g_assert_cmpstr(test->m_activeURI.data(), !=, webkit_web_resource_get_uri(test->m_resource.get()));
        test->m_activeURI = webkit_web_resource_get_uri(test->m_resource.get());
    }

    void resourceLoadStarted(WebKitWebResource* resource, WebKitURIRequest* request)
    {
        if (resource == webkit_web_view_get_main_resource(m_webView))
            return;

        m_resource = resource;
        m_activeURI = webkit_web_resource_get_uri(resource);
        checkActiveURI("/redirected.css");
        g_assert_cmpstr(m_activeURI.data(), ==, webkit_uri_request_get_uri(request));
        g_signal_connect(resource, "notify::uri", G_CALLBACK(uriChanged), this);
    }

    void resourceSentRequest(WebKitWebResource* resource, WebKitURIRequest* request, WebKitURIResponse* redirectResponse)
    {
        if (resource != m_resource)
            return;

        if (redirectResponse)
            checkActiveURI("/simple-style.css");
        else
            checkActiveURI("/redirected.css");
        g_assert_cmpstr(m_activeURI.data(), ==, webkit_uri_request_get_uri(request));
    }

    void resourceReceivedResponse(WebKitWebResource* resource)
    {
        if (resource != m_resource)
            return;

        checkActiveURI("/simple-style.css");
    }

    void resourceReceivedData(WebKitWebResource* resource, guint64 bytesReceived)
    {
    }

    void resourceFinished(WebKitWebResource* resource)
    {
        if (resource == m_resource)
            checkActiveURI("/simple-style.css");
        ResourcesTest::resourceFinished(resource);
    }

    void resourceFailed(WebKitWebResource*, GError*)
    {
        g_assert_not_reached();
    }

    CString m_activeURI;

private:
    void checkActiveURI(const char* uri)
    {
        ASSERT_CMP_CSTRING(m_activeURI, ==, kServer->getURIForPath(uri));
    }
};

static void testWebResourceActiveURI(ResourceURITrackingTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/redirected-css.html").data());
    test->waitUntilResourceLoadFinished();
}

static void testWebResourceGetData(ResourcesTest* test, gconstpointer)
{
    test->loadURI(kServer->getURIForPath("/").data());
    test->waitUntilResourcesLoaded(4);

    WebKitWebResource* resource = webkit_web_view_get_main_resource(test->m_webView);
    g_assert_nonnull(resource);
    test->checkResourceData(resource);

    GList* subresources = test->subresources();
    for (GList* item = subresources; item; item = g_list_next(item))
        test->checkResourceData(WEBKIT_WEB_RESOURCE(item->data));
}

static void webViewLoadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent, GMainLoop* mainLoop)
{
    if (loadEvent != WEBKIT_LOAD_FINISHED)
        return;
    g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(webViewLoadChanged), mainLoop);
    g_main_loop_quit(mainLoop);
}

static void testWebResourceGetDataError(Test* test, gconstpointer)
{
    GRefPtr<GMainLoop> mainLoop = adoptGRef(g_main_loop_new(nullptr, FALSE));
    GRefPtr<WebKitWebView> webView = WEBKIT_WEB_VIEW(Test::createWebView(test->m_webContext.get()));
    webkit_web_view_load_html(webView.get(), "<html></html>", nullptr);
    g_signal_connect(webView.get(), "load-changed", G_CALLBACK(webViewLoadChanged), mainLoop.get());
    g_main_loop_run(mainLoop.get());

    auto* resource = webkit_web_view_get_main_resource(webView.get());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(resource));
    webkit_web_resource_get_data(resource, nullptr, [](GObject* source, GAsyncResult* result, gpointer userData) {
        size_t dataSize;
        GUniqueOutPtr<GError> error;
        auto* data = webkit_web_resource_get_data_finish(WEBKIT_WEB_RESOURCE(source), result, &dataSize, &error.outPtr());
        g_assert_null(data);
        g_assert_error(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED);
        g_main_loop_quit(static_cast<GMainLoop*>(userData));
    }, mainLoop.get());
    webView = nullptr;
    g_main_loop_run(mainLoop.get());
}

static void testWebResourceGetDataEmpty(Test* test, gconstpointer)
{
    GRefPtr<GMainLoop> mainLoop = adoptGRef(g_main_loop_new(nullptr, FALSE));
    GRefPtr<WebKitWebView> webView = WEBKIT_WEB_VIEW(Test::createWebView(test->m_webContext.get()));
    webkit_web_view_load_html(webView.get(), "", nullptr);
    g_signal_connect(webView.get(), "load-changed", G_CALLBACK(webViewLoadChanged), mainLoop.get());
    g_main_loop_run(mainLoop.get());

    auto* resource = webkit_web_view_get_main_resource(webView.get());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(resource));
    webkit_web_resource_get_data(resource, nullptr, [](GObject* source, GAsyncResult* result, gpointer userData) {
        size_t dataSize;
        GUniqueOutPtr<GError> error;
        auto* data = webkit_web_resource_get_data_finish(WEBKIT_WEB_RESOURCE(source), result, &dataSize, &error.outPtr());
        g_assert_nonnull(data);
        g_assert_cmpuint(dataSize, ==, 1);
        g_assert_cmpint(data[0], ==, '\0');
        g_assert_no_error(error.get());
        g_main_loop_quit(static_cast<GMainLoop*>(userData));
    }, mainLoop.get());
    g_main_loop_run(mainLoop.get());
}

static void testWebViewResourcesHistoryCache(SingleResourceLoadTest* test, gconstpointer)
{
    CString javascriptURI = kServer->getURIForPath("/javascript.html");
    test->loadURI(javascriptURI.data());
    test->waitUntilResourceLoadFinished();
    WebKitWebResource* resource = webkit_web_view_get_main_resource(test->m_webView);
    g_assert_nonnull(resource);
    g_assert_cmpstr(webkit_web_resource_get_uri(resource), ==, javascriptURI.data());

    CString simpleStyleCSSURI = kServer->getURIForPath("/simple-style-css.html");
    test->loadURI(simpleStyleCSSURI.data());
    test->waitUntilResourceLoadFinished();
    resource = webkit_web_view_get_main_resource(test->m_webView);
    g_assert_nonnull(resource);
    g_assert_cmpstr(webkit_web_resource_get_uri(resource), ==, simpleStyleCSSURI.data());

    test->goBack();
    test->waitUntilResourceLoadFinished();
    resource = webkit_web_view_get_main_resource(test->m_webView);
    g_assert_nonnull(resource);
    g_assert_cmpstr(webkit_web_resource_get_uri(resource), ==, javascriptURI.data());

    test->goForward();
    test->waitUntilResourceLoadFinished();
    resource = webkit_web_view_get_main_resource(test->m_webView);
    g_assert_nonnull(resource);
    g_assert_cmpstr(webkit_web_resource_get_uri(resource), ==, simpleStyleCSSURI.data());
}

class SendRequestTest: public SingleResourceLoadTest {
public:
    MAKE_GLIB_TEST_FIXTURE(SendRequestTest);

    void resourceSentRequest(WebKitWebResource* resource, WebKitURIRequest* request, WebKitURIResponse* redirectResponse)
    {
        if (resource != m_resource)
            return;

        if (redirectResponse)
            g_assert_cmpstr(webkit_uri_request_get_uri(request), ==, m_expectedNewResourceURIAfterRedirection.data());
        else
            g_assert_cmpstr(webkit_uri_request_get_uri(request), ==, m_expectedNewResourceURI.data());
        g_assert_cmpstr(webkit_uri_request_get_uri(request), ==, webkit_web_resource_get_uri(resource));

        SingleResourceLoadTest::resourceSentRequest(resource, request, redirectResponse);
    }

    void resourceFailed(WebKitWebResource* resource, GError* error)
    {
        if (resource != m_resource)
            return;

        g_assert_cmpstr(webkit_web_resource_get_uri(resource), ==, m_expectedCancelledResourceURI.data());
        g_assert_error(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED);

        SingleResourceLoadTest::resourceFailed(resource, error);
    }

    void setExpectedNewResourceURI(const CString& uri)
    {
        m_expectedNewResourceURI = uri;
    }

    void setExpectedCancelledResourceURI(const CString& uri)
    {
        m_expectedCancelledResourceURI = uri;
    }

    void setExpectedNewResourceURIAfterRedirection(const CString& uri)
    {
        m_expectedNewResourceURIAfterRedirection = uri;
    }

    CString m_expectedNewResourceURI;
    CString m_expectedCancelledResourceURI;
    CString m_expectedNewResourceURIAfterRedirection;
};

static void testWebResourceSendRequest(SendRequestTest* test, gconstpointer)
{
    test->setExpectedNewResourceURI(kServer->getURIForPath("/javascript.js"));
    test->loadURI(kServer->getURIForPath("relative-javascript.html").data());
    test->waitUntilResourceLoadFinished();
    g_assert_nonnull(test->m_resource);

    Vector<SingleResourceLoadTest::LoadEvents>& events = test->m_loadEvents;
    g_assert_cmpint(events.size(), ==, 5);
    g_assert_cmpint(events[0], ==, SingleResourceLoadTest::Started);
    g_assert_cmpint(events[1], ==, SingleResourceLoadTest::SentRequest);
    g_assert_cmpint(events[2], ==, SingleResourceLoadTest::ReceivedResponse);
    g_assert_cmpint(events[3], ==, SingleResourceLoadTest::ReceivedData);
    g_assert_cmpint(events[4], ==, SingleResourceLoadTest::Finished);
    events.clear();

    // Cancel request.
    test->setExpectedCancelledResourceURI(kServer->getURIForPath("/cancel-this.js"));
    test->loadURI(kServer->getURIForPath("/resource-to-cancel.html").data());
    test->waitUntilResourceLoadFinished();
    g_assert_nonnull(test->m_resource);

    g_assert_cmpint(events.size(), ==, 3);
    g_assert_cmpint(events[0], ==, SingleResourceLoadTest::Started);
    g_assert_cmpint(events[1], ==, SingleResourceLoadTest::Failed);
    g_assert_cmpint(events[2], ==, SingleResourceLoadTest::Finished);
    events.clear();

    // URI changed after a redirect.
    test->setExpectedNewResourceURI(kServer->getURIForPath("/redirected.js"));
    test->setExpectedNewResourceURIAfterRedirection(kServer->getURIForPath("/javascript-after-redirection.js"));
    test->loadURI(kServer->getURIForPath("redirected-javascript.html").data());
    test->waitUntilResourceLoadFinished();
    g_assert_nonnull(test->m_resource);

    g_assert_cmpint(events.size(), ==, 6);
    g_assert_cmpint(events[0], ==, SingleResourceLoadTest::Started);
    g_assert_cmpint(events[1], ==, SingleResourceLoadTest::SentRequest);
    g_assert_cmpint(events[2], ==, SingleResourceLoadTest::Redirected);
    g_assert_cmpint(events[3], ==, SingleResourceLoadTest::ReceivedResponse);
    g_assert_cmpint(events[4], ==, SingleResourceLoadTest::ReceivedData);
    g_assert_cmpint(events[5], ==, SingleResourceLoadTest::Finished);
    events.clear();

    // Cancel after a redirect.
    test->setExpectedNewResourceURI(kServer->getURIForPath("/redirected-to-cancel.js"));
    test->setExpectedCancelledResourceURI(kServer->getURIForPath("/redirected-to-cancel.js"));
    test->loadURI(kServer->getURIForPath("/redirected-to-cancel.html").data());
    test->waitUntilResourceLoadFinished();
    g_assert_nonnull(test->m_resource);

    g_assert_cmpint(events.size(), ==, 4);
    g_assert_cmpint(events[0], ==, SingleResourceLoadTest::Started);
    g_assert_cmpint(events[1], ==, SingleResourceLoadTest::SentRequest);
    g_assert_cmpint(events[2], ==, SingleResourceLoadTest::Failed);
    g_assert_cmpint(events[3], ==, SingleResourceLoadTest::Finished);
    events.clear();
}

static GMutex s_serverMutex;
static const unsigned s_maxConnectionsPerHost = 6;

class SyncRequestOnMaxConnsTest: public ResourcesTest {
public:
    MAKE_GLIB_TEST_FIXTURE(SyncRequestOnMaxConnsTest);

    void resourceLoadStarted(WebKitWebResource*, WebKitURIRequest*) override
    {
        if (!m_resourcesToStartPending)
            return;

        if (!--m_resourcesToStartPending)
            g_main_loop_quit(m_mainLoop);
    }

    void waitUntilResourcesStarted(unsigned requestCount)
    {
        m_resourcesToStartPending = requestCount;
        g_main_loop_run(m_mainLoop);
    }

    unsigned m_resourcesToStartPending;
};

static void testWebViewSyncRequestOnMaxConns(SyncRequestOnMaxConnsTest* test, gconstpointer)
{
    WTF::GMutexLocker<GMutex> lock(s_serverMutex);
    test->loadURI(kServer->getURIForPath("/sync-request-on-max-conns-0").data());
    test->waitUntilResourcesStarted(s_maxConnectionsPerHost + 1); // s_maxConnectionsPerHost resource + main resource.

    for (unsigned i = 0; i < 2; ++i) {
        GUniquePtr<char> xhr(g_strdup_printf("xhr = new XMLHttpRequest; xhr.open('GET', '/sync-request-on-max-conns-xhr%u', false); xhr.send();", i));
        webkit_web_view_run_javascript(test->m_webView, xhr.get(), nullptr, nullptr, nullptr);
    }

    // By default sync XHRs have a 10 seconds timeout, we don't want to wait all that so use our own timeout.
    guint timeoutSourceID = g_timeout_add(1000, [] (gpointer) -> gboolean {
        g_assert_not_reached();
        return G_SOURCE_REMOVE;
    }, nullptr);

    struct UnlockServerSourceContext {
        WTF::GMutexLocker<GMutex>& lock;
        guint unlockServerSourceID;
    } context = {
        lock,
        g_idle_add_full(G_PRIORITY_DEFAULT, [](gpointer userData) -> gboolean {
            auto& context = *static_cast<UnlockServerSourceContext*>(userData);
            context.unlockServerSourceID = 0;
            context.lock.unlock();
            return G_SOURCE_REMOVE;
        }, &context, nullptr)
    };
    test->waitUntilResourcesLoaded(s_maxConnectionsPerHost + 3); // s_maxConnectionsPerHost resource + main resource + 2 XHR.
    g_source_remove(timeoutSourceID);
    if (context.unlockServerSourceID)
        g_source_remove(context.unlockServerSourceID);
}

#if USE(SOUP2)
static void addCacheHTTPHeadersToResponse(SoupMessage* message)
{
    // The actual date doesn't really matter.
    SoupDate* soupDate = soup_date_new_from_now(0);
    GUniquePtr<char> date(soup_date_to_string(soupDate, SOUP_DATE_HTTP));
    soup_message_headers_append(message->response_headers, "Last-Modified", date.get());
    soup_date_free(soupDate);
    soup_message_headers_append(message->response_headers, "Cache-control", "public, max-age=31536000");
    soupDate = soup_date_new_from_now(3600);
    date.reset(soup_date_to_string(soupDate, SOUP_DATE_HTTP));
    soup_message_headers_append(message->response_headers, "Expires", date.get());
    soup_date_free(soupDate);
}
#else
static void addCacheHTTPHeadersToResponse(SoupServerMessage* message)
{
    // The actual date doesn't really matter.
    GRefPtr<GDateTime> dateTime = adoptGRef(g_date_time_new_now_local());
    GUniquePtr<char> date(soup_date_time_to_string(dateTime.get(), SOUP_DATE_HTTP));
    auto* responseHeaders = soup_server_message_get_response_headers(message);
    soup_message_headers_append(responseHeaders, "Last-Modified", date.get());
    soup_message_headers_append(responseHeaders, "Cache-control", "public, max-age=31536000");
    dateTime = adoptGRef(g_date_time_add_seconds(dateTime.get(), 3600));
    date.reset(soup_date_time_to_string(dateTime.get(), SOUP_DATE_HTTP));
    soup_message_headers_append(responseHeaders, "Expires", date.get());
}
#endif

#if USE(SOUP2)
static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
#else
static void serverCallback(SoupServer* server, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
#endif
{
    if (soup_server_message_get_method(message) != SOUP_METHOD_GET) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED, nullptr);
        return;
    }

    auto* responseBody = soup_server_message_get_response_body(message);
    soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);

    if (soup_message_headers_get_one(soup_server_message_get_request_headers(message), "If-Modified-Since")) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_MODIFIED, nullptr);
        soup_message_body_complete(responseBody);
        return;
    }

    auto* responseHeaders = soup_server_message_get_response_headers(message);

    if (g_str_equal(path, "/")) {
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, kIndexHtml, strlen(kIndexHtml));
    } else if (g_str_equal(path, "/javascript.html")) {
        static const char* javascriptHtml = "<html><head><script language='javascript' src='/javascript.js'></script></head><body></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, javascriptHtml, strlen(javascriptHtml));
    } else if (g_str_equal(path, "/image.html")) {
        static const char* imageHTML = "<html><body><img src='/blank.ico'></img></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, imageHTML, strlen(imageHTML));
    } else if (g_str_equal(path, "/redirected-css.html")) {
        static const char* redirectedCSSHtml = "<html><head><link rel='stylesheet' href='/redirected.css' type='text/css'></head><body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, redirectedCSSHtml, strlen(redirectedCSSHtml));
    } else if (g_str_equal(path, "/invalid-css.html")) {
        static const char* invalidCSSHtml = "<html><head><link rel='stylesheet' href='http://127.0.0.1:1234/invalid.css' type='text/css'></head><body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, invalidCSSHtml, strlen(invalidCSSHtml));
    } else if (g_str_equal(path, "/simple-style-css.html")) {
        static const char* simpleStyleCSSHtml = "<html><head><link rel='stylesheet' href='/simple-style.css' type='text/css'></head><body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, simpleStyleCSSHtml, strlen(simpleStyleCSSHtml));
    } else if (g_str_equal(path, "/style.css")) {
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, kStyleCSS, strlen(kStyleCSS));
        addCacheHTTPHeadersToResponse(message);
        soup_message_headers_append(responseHeaders, "Content-Type", "text/css");
    } else if (g_str_equal(path, "/javascript.js") || g_str_equal(path, "/javascript-after-redirection.js")) {
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, kJavascript, strlen(kJavascript));
        soup_message_headers_append(responseHeaders, "Content-Type", "text/javascript");
        soup_message_headers_append(responseHeaders, "Content-Disposition", "attachment; filename=JavaScript.js");
    } else if (g_str_equal(path, "/relative-javascript.html")) {
        static const char* javascriptRelativeHTML = "<html><head><script language='javascript' src='remove-this/javascript.js'></script></head><body></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, javascriptRelativeHTML, strlen(javascriptRelativeHTML));
    } else if (g_str_equal(path, "/resource-to-cancel.html")) {
        static const char* resourceToCancelHTML = "<html><head><script language='javascript' src='cancel-this.js'></script></head><body></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, resourceToCancelHTML, strlen(resourceToCancelHTML));
    } else if (g_str_equal(path, "/redirected-javascript.html")) {
        static const char* javascriptRelativeHTML = "<html><head><script language='javascript' src='/redirected.js'></script></head><body></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, javascriptRelativeHTML, strlen(javascriptRelativeHTML));
    } else if (g_str_equal(path, "/redirected-to-cancel.html")) {
        static const char* javascriptRelativeHTML = "<html><head><script language='javascript' src='/redirected-to-cancel.js'></script></head><body></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, javascriptRelativeHTML, strlen(javascriptRelativeHTML));
    } else if (g_str_equal(path, "/blank.ico")) {
        GUniquePtr<char> filePath(g_build_filename(Test::getResourcesDir().data(), path, nullptr));
        char* contents;
        gsize contentsLength;
        g_file_get_contents(filePath.get(), &contents, &contentsLength, 0);
        soup_message_body_append(responseBody, SOUP_MEMORY_TAKE, contents, contentsLength);
        addCacheHTTPHeadersToResponse(message);
        soup_message_headers_append(responseHeaders, "Content-Type", "image/vnd.microsoft.icon");
    } else if (g_str_equal(path, "/simple-style.css")) {
        static const char* simpleCSS =
            "body {"
            "    margin: 0px;"
            "    padding: 0px;"
            "}";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, simpleCSS, strlen(simpleCSS));
        soup_message_headers_append(responseHeaders, "Content-Type", "text/css");
    } else if (g_str_equal(path, "/redirected.css")) {
        soup_server_message_set_status(message, SOUP_STATUS_MOVED_PERMANENTLY, nullptr);
        soup_message_headers_append(responseHeaders, "Location", "/simple-style.css");
    } else if (g_str_equal(path, "/redirected.js")) {
        soup_server_message_set_status(message, SOUP_STATUS_MOVED_PERMANENTLY, nullptr);
        soup_message_headers_append(responseHeaders, "Location", "/remove-this/javascript-after-redirection.js");
    } else if (g_str_equal(path, "/redirected-to-cancel.js")) {
        soup_server_message_set_status(message, SOUP_STATUS_MOVED_PERMANENTLY, nullptr);
        soup_message_headers_append(responseHeaders, "Location", "/cancel-this.js");
    } else if (g_str_equal(path, "/iframe-no-content.html")) {
        static const char* iframeNoContentHTML = "<html><body><iframe src='/no-content'/></body></html>";
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, iframeNoContentHTML, strlen(iframeNoContentHTML));
    } else if (g_str_equal(path, "/no-content")) {
        soup_server_message_set_status(message, SOUP_STATUS_NO_CONTENT, nullptr);
    } else if (g_str_has_prefix(path, "/sync-request-on-max-conns-")) {
        char* contents;
        gsize contentsLength;
        if (g_str_equal(path, "/sync-request-on-max-conns-0")) {
            GString* imagesHTML = g_string_new("<html><body>");
            for (unsigned i = 1; i <= s_maxConnectionsPerHost; ++i)
                g_string_append_printf(imagesHTML, "<img src='/sync-request-on-max-conns-%u'>", i);
            g_string_append(imagesHTML, "</body></html>");

            contentsLength = imagesHTML->len;
            contents = g_string_free(imagesHTML, FALSE);
        } else {
            {
                // We don't actually need to keep the mutex, so we release it as soon as we get it.
                WTF::GMutexLocker<GMutex> lock(s_serverMutex);
            }

            GUniquePtr<char> filePath(g_build_filename(Test::getResourcesDir().data(), "blank.ico", nullptr));
            g_file_get_contents(filePath.get(), &contents, &contentsLength, 0);
        }
        soup_message_body_append(responseBody, SOUP_MEMORY_TAKE, contents, contentsLength);
    } else
        soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
    soup_message_body_complete(responseBody);
}

void beforeAll()
{
    kServer = new WebKitTestServer(WebKitTestServer::ServerOptions::ServerRunInThread);
    kServer->run(serverCallback);

    ResourcesTest::add("WebKitWebView", "resources", testWebViewResources);
    SingleResourceLoadTest::add("WebKitWebResource", "loading", testWebResourceLoading);
    SingleResourceLoadTest::add("WebKitWebResource", "response", testWebResourceResponse);
    SingleResourceLoadTest::add("WebKitWebResource", "mime-type", testWebResourceMimeType);
    SingleResourceLoadTest::add("WebKitWebResource", "suggested-filename", testWebResourceSuggestedFilename);
    ResourceURITrackingTest::add("WebKitWebResource", "active-uri", testWebResourceActiveURI);
    ResourcesTest::add("WebKitWebResource", "get-data", testWebResourceGetData);
    Test::add("WebKitWebResource", "get-data-error", testWebResourceGetDataError);
    Test::add("WebKitWebResource", "get-data-empty", testWebResourceGetDataEmpty);
    SingleResourceLoadTest::add("WebKitWebView", "history-cache", testWebViewResourcesHistoryCache);
    SendRequestTest::add("WebKitWebPage", "send-request", testWebResourceSendRequest);
    SyncRequestOnMaxConnsTest::add("WebKitWebView", "sync-request-on-max-conns", testWebViewSyncRequestOnMaxConns);
}

void afterAll()
{
    delete kServer;
}
