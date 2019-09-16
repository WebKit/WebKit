/*
 * Copyright (C) 2011 Igalia S.L.
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

static void loadChangedCallback(WebKitWebView* webView, WebKitLoadEvent loadEvent, LoadTrackingTest* test)
{
    switch (loadEvent) {
    case WEBKIT_LOAD_STARTED:
        g_assert_true(webkit_web_view_is_loading(webView));
        g_assert_cmpstr(test->m_activeURI.data(), ==, webkit_web_view_get_uri(webView));
        test->provisionalLoadStarted();
        break;
    case WEBKIT_LOAD_REDIRECTED:
        g_assert_true(webkit_web_view_is_loading(webView));
        test->m_activeURI = webkit_web_view_get_uri(webView);
        test->m_committedURI = test->m_activeURI;
        if (!test->m_redirectURI.isNull())
            g_assert_cmpstr(test->m_redirectURI.data(), ==, test->m_activeURI.data());
        test->provisionalLoadReceivedServerRedirect();
        break;
    case WEBKIT_LOAD_COMMITTED: {
        g_assert_true(webkit_web_view_is_loading(webView));
        test->m_activeURI = webkit_web_view_get_uri(webView);

        // Check that on committed we always have a main resource with a response.
        WebKitWebResource* resource = webkit_web_view_get_main_resource(webView);
        g_assert_nonnull(resource);
        g_assert_nonnull(webkit_web_resource_get_response(resource));

        test->loadCommitted();
        break;
    }
    case WEBKIT_LOAD_FINISHED:
        if (!test->m_loadFailed)
            g_assert_cmpstr(test->m_activeURI.data(), ==, webkit_web_view_get_uri(webView));

        // When a new load is started before the previous one has finished, we receive the load-finished signal
        // of the ongoing load while we already have a provisional URL for the new load. This is the only case
        // where isloading is true when the load has finished.
        if (test->m_activeURI == test->m_committedURI)
            g_assert_false(webkit_web_view_is_loading(webView));

        test->loadFinished();
        break;
    default:
        g_assert_not_reached();
    }
}

static void loadFailedCallback(WebKitWebView* webView, WebKitLoadEvent loadEvent, const char* failingURI, GError* error, LoadTrackingTest* test)
{
    test->m_loadFailed = true;

    g_assert_nonnull(error);
    test->m_error.reset(g_error_copy(error));

    if (!g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED)) {
        // When a new load is started before the previous one has finished, we receive the load-failed signal
        // of the ongoing load while we already have a provisional URL for the new load. This is the only case
        // where is-loading is true when the load has failed.
        g_assert_false(webkit_web_view_is_loading(webView));
    }

    switch (loadEvent) {
    case WEBKIT_LOAD_STARTED:
        g_assert_cmpstr(test->m_activeURI.data(), ==, failingURI);
        test->provisionalLoadFailed(failingURI, error);
        break;
    case WEBKIT_LOAD_COMMITTED:
        g_assert_cmpstr(test->m_activeURI.data(), ==, webkit_web_view_get_uri(webView));
        test->loadFailed(failingURI, error);
        break;
    default:
        g_assert_not_reached();
    }
}

static gboolean loadFailedWithTLSErrorsCallback(WebKitWebView* webView, const char* failingURI, GTlsCertificate* certificate, GTlsCertificateFlags tlsErrors, LoadTrackingTest* test)
{
    test->m_loadFailed = true;
    g_assert_false(webkit_web_view_is_loading(webView));
    g_assert_cmpstr(test->m_activeURI.data(), ==, failingURI);
    g_assert_true(G_IS_TLS_CERTIFICATE(certificate));
    g_assert_cmpuint(tlsErrors, !=, 0);
    return test->loadFailedWithTLSErrors(failingURI, certificate, tlsErrors);
}

static void estimatedProgressChangedCallback(GObject*, GParamSpec*, LoadTrackingTest* test)
{
    test->estimatedProgressChanged();
}

LoadTrackingTest::LoadTrackingTest()
    : m_runLoadUntilCompletion(false)
    , m_loadFailed(false)
{
    g_signal_connect(m_webView, "load-changed", G_CALLBACK(loadChangedCallback), this);
    g_signal_connect(m_webView, "load-failed", G_CALLBACK(loadFailedCallback), this);
    g_signal_connect(m_webView, "load-failed-with-tls-errors", G_CALLBACK(loadFailedWithTLSErrorsCallback), this);
    g_signal_connect(m_webView, "notify::estimated-load-progress", G_CALLBACK(estimatedProgressChangedCallback), this);

    g_assert_null(webkit_web_view_get_uri(m_webView));
}

LoadTrackingTest::~LoadTrackingTest()
{
    g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
}

void LoadTrackingTest::waitUntilLoadFinished()
{
    m_estimatedProgress = 0;
    m_runLoadUntilCompletion = true;
    g_main_loop_run(m_mainLoop);
}

void LoadTrackingTest::provisionalLoadStarted()
{
    m_loadEvents.append(ProvisionalLoadStarted);
}

void LoadTrackingTest::provisionalLoadReceivedServerRedirect()
{
    m_loadEvents.append(ProvisionalLoadReceivedServerRedirect);
}

void LoadTrackingTest::provisionalLoadFailed(const gchar* failingURI, GError* error)
{
    m_loadEvents.append(ProvisionalLoadFailed);
}

void LoadTrackingTest::loadCommitted()
{
    m_loadEvents.append(LoadCommitted);
}

void LoadTrackingTest::loadFinished()
{
    m_loadEvents.append(LoadFinished);
    if (m_runLoadUntilCompletion)
        g_main_loop_quit(m_mainLoop);
}

void LoadTrackingTest::loadFailed(const gchar* failingURI, GError* error)
{
    m_loadEvents.append(LoadFailed);
}

bool LoadTrackingTest::loadFailedWithTLSErrors(const gchar* /*failingURI*/, GTlsCertificate*, GTlsCertificateFlags)
{
    m_loadEvents.append(LoadFailedWithTLSErrors);
    return false;
}

void LoadTrackingTest::estimatedProgressChanged()
{
    double progress = webkit_web_view_get_estimated_load_progress(m_webView);
    g_assert_cmpfloat(m_estimatedProgress, <, progress);
    m_estimatedProgress = progress;
}

void LoadTrackingTest::loadURI(const char* uri)
{
    reset();
    WebViewTest::loadURI(uri);
}

void LoadTrackingTest::loadHtml(const char* html, const char* baseURI)
{
    reset();
    WebViewTest::loadHtml(html, baseURI);
}

void LoadTrackingTest::loadPlainText(const char* plainText)
{
    reset();
    WebViewTest::loadPlainText(plainText);
}

void LoadTrackingTest::loadBytes(GBytes* bytes, const char* mimeType, const char* encoding, const char* baseURI)
{
    reset();
    WebViewTest::loadBytes(bytes, mimeType, encoding, baseURI);
}

void LoadTrackingTest::loadRequest(WebKitURIRequest* request)
{
    reset();
    WebViewTest::loadRequest(request);
}

void LoadTrackingTest::reload()
{
    reset();
    webkit_web_view_reload(m_webView);
}

void LoadTrackingTest::goBack()
{
    reset();
    WebViewTest::goBack();
}

void LoadTrackingTest::goForward()
{
    reset();
    WebViewTest::goForward();
}

void LoadTrackingTest::loadAlternateHTML(const char* html, const char* contentURI, const char* baseURI)
{
    reset();
    WebViewTest::loadAlternateHTML(html, contentURI, baseURI);
}

void LoadTrackingTest::reset()
{
    m_runLoadUntilCompletion = false;
    m_loadFailed = false;
    m_loadEvents.clear();
    m_estimatedProgress = 0;
    m_error.reset();
    m_committedURI = { };
}
