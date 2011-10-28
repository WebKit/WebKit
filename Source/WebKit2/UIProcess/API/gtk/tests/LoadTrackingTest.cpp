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

#include <webkit2/webkit2.h>

static gboolean provisionalLoadStartedCallback(WebKitWebLoaderClient* client, WebKitWebView*, LoadTrackingTest* test)
{
    g_assert_cmpstr(test->m_activeURI.data(), ==, webkit_web_view_get_uri(test->m_webView));
    test->provisionalLoadStarted(client);
    return TRUE;
}

static gboolean provisionalLoadReceivedServerRedirectCallback(WebKitWebLoaderClient* client, WebKitWebView*, LoadTrackingTest* test)
{
    test->m_activeURI = webkit_web_view_get_uri(test->m_webView);
    if (!test->m_redirectURI.isNull())
        g_assert_cmpstr(test->m_redirectURI.data(), ==, test->m_activeURI.data());
    test->provisionalLoadReceivedServerRedirect(client);
    return TRUE;
}

static gboolean provisionalLoadFailedCallback(WebKitWebLoaderClient* client, WebKitWebView*, const gchar* failingURI, GError* error, LoadTrackingTest* test)
{
    g_assert_cmpstr(test->m_activeURI.data(), ==, webkit_web_view_get_uri(test->m_webView));
    g_assert(error);
    test->provisionalLoadFailed(client, failingURI, error);
    return TRUE;
}

static gboolean loadCommittedCallback(WebKitWebLoaderClient* client, WebKitWebView*, LoadTrackingTest* test)
{
    g_assert_cmpstr(test->m_activeURI.data(), ==, webkit_web_view_get_uri(test->m_webView));
    test->loadCommitted(client);
    return TRUE;
}

static gboolean loadFinishedCallback(WebKitWebLoaderClient* client, WebKitWebView*, LoadTrackingTest* test)
{
    g_assert_cmpstr(test->m_activeURI.data(), ==, webkit_web_view_get_uri(test->m_webView));
    test->loadFinished(client);
    return TRUE;
}

static gboolean loadFailedCallback(WebKitWebLoaderClient* client, WebKitWebView*, const gchar* failingURI, GError* error, LoadTrackingTest* test)
{
    g_assert_cmpstr(test->m_activeURI.data(), ==, webkit_web_view_get_uri(test->m_webView));
    g_assert(error);
    test->loadFailed(client, failingURI, error);
    return TRUE;
}

static void estimatedProgressChangedCallback(GObject*, GParamSpec*, LoadTrackingTest* test)
{
    test->estimatedProgressChanged();
}

LoadTrackingTest::LoadTrackingTest()
    : m_runLoadUntilCompletion(false)
{
    WebKitWebLoaderClient* client = webkit_web_view_get_loader_client(m_webView);
    g_signal_connect(client, "provisional-load-started", G_CALLBACK(provisionalLoadStartedCallback), this);
    g_signal_connect(client, "provisional-load-received-server-redirect", G_CALLBACK(provisionalLoadReceivedServerRedirectCallback), this);
    g_signal_connect(client, "provisional-load-failed", G_CALLBACK(provisionalLoadFailedCallback), this);
    g_signal_connect(client, "load-committed", G_CALLBACK(loadCommittedCallback), this);
    g_signal_connect(client, "load-finished", G_CALLBACK(loadFinishedCallback), this);
    g_signal_connect(client, "load-failed", G_CALLBACK(loadFailedCallback), this);
    g_signal_connect(m_webView, "notify::estimated-load-progress", G_CALLBACK(estimatedProgressChangedCallback), this);

    g_assert(!webkit_web_view_get_uri(m_webView));
}

LoadTrackingTest::~LoadTrackingTest()
{
    WebKitWebLoaderClient* client = webkit_web_view_get_loader_client(m_webView);
    g_signal_handlers_disconnect_matched(client, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
    g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
}

void LoadTrackingTest::waitUntilLoadFinished()
{
    m_estimatedProgress = 0;
    m_runLoadUntilCompletion = true;
    g_main_loop_run(m_mainLoop);
}

void LoadTrackingTest::provisionalLoadStarted(WebKitWebLoaderClient* client)
{
    m_loadEvents.append(ProvisionalLoadStarted);
}

void LoadTrackingTest::provisionalLoadReceivedServerRedirect(WebKitWebLoaderClient* client)
{
    m_loadEvents.append(ProvisionalLoadReceivedServerRedirect);
}

void LoadTrackingTest::provisionalLoadFailed(WebKitWebLoaderClient* client, const gchar* failingURI, GError* error)
{
    m_loadEvents.append(ProvisionalLoadFailed);
    if (m_runLoadUntilCompletion)
        g_main_loop_quit(m_mainLoop);
}

void LoadTrackingTest::loadCommitted(WebKitWebLoaderClient* client)
{
    m_loadEvents.append(LoadCommitted);
}

void LoadTrackingTest::loadFinished(WebKitWebLoaderClient* client)
{
    m_loadEvents.append(LoadFinished);
    if (m_runLoadUntilCompletion)
        g_main_loop_quit(m_mainLoop);
}

void LoadTrackingTest::loadFailed(WebKitWebLoaderClient* client, const gchar* failingURI, GError* error)
{
    m_loadEvents.append(LoadFailed);
    if (m_runLoadUntilCompletion)
        g_main_loop_quit(m_mainLoop);
}

void LoadTrackingTest::estimatedProgressChanged()
{
    double progress = webkit_web_view_get_estimated_load_progress(m_webView);
    g_assert_cmpfloat(m_estimatedProgress, <, progress);
    m_estimatedProgress = progress;
}

