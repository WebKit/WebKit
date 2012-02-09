/*
 * Copyright (C) 2011 Igalia S.L.
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
#include "WebViewTest.h"

#include <WebCore/GOwnPtrGtk.h>

WebViewTest::WebViewTest()
    : m_webView(WEBKIT_WEB_VIEW(g_object_ref_sink(webkit_web_view_new())))
    , m_mainLoop(g_main_loop_new(0, TRUE))
    , m_parentWindow(0)
{
    assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_webView));
}

WebViewTest::~WebViewTest()
{
    if (m_parentWindow)
        gtk_widget_destroy(m_parentWindow);
    g_object_unref(m_webView);
    g_main_loop_unref(m_mainLoop);
}

static gboolean testLoadTimeoutFinishLoop(GMainLoop* loop)
{
    g_main_loop_quit(loop);
    return FALSE;
}

void WebViewTest::loadURI(const char* uri)
{
    m_activeURI = uri;
    webkit_web_view_load_uri(m_webView, uri);
}

void WebViewTest::loadHtml(const char* html, const char* baseURI)
{
    if (!baseURI)
        m_activeURI = "about:blank";
    else
        m_activeURI = baseURI;
    webkit_web_view_load_html(m_webView, html, baseURI);
}

void WebViewTest::loadPlainText(const char* plainText)
{
    m_activeURI = "about:blank";
    webkit_web_view_load_plain_text(m_webView, plainText);
}

void WebViewTest::loadRequest(WebKitURIRequest* request)
{
    m_activeURI = webkit_uri_request_get_uri(request);
    webkit_web_view_load_request(m_webView, request);
}

void WebViewTest::replaceContent(const char* html, const char* contentURI, const char* baseURI)
{
    // FIXME: The active uri should be the contentURI,
    // but WebPageProxy doesn't return the unreachableURL
    // when the page has been loaded with AlternateHTML()
    // See https://bugs.webkit.org/show_bug.cgi?id=75465.
#if 0
    m_activeURI = contentURI;
#else
    m_activeURI = "about:blank";
#endif
    webkit_web_view_replace_content(m_webView, html, contentURI, baseURI);
}

void WebViewTest::goBack()
{
    if (webkit_web_view_can_go_back(m_webView)) {
        WebKitBackForwardList* list = webkit_web_view_get_back_forward_list(m_webView);
        WebKitBackForwardListItem* item = webkit_back_forward_list_get_nth_item(list, -1);
        m_activeURI = webkit_back_forward_list_item_get_original_uri(item);
    }

    // Call go_back even when can_go_back returns FALSE to check nothing happens.
    webkit_web_view_go_back(m_webView);
}

void WebViewTest::goForward()
{
    if (webkit_web_view_can_go_forward(m_webView)) {
        WebKitBackForwardList* list = webkit_web_view_get_back_forward_list(m_webView);
        WebKitBackForwardListItem* item = webkit_back_forward_list_get_nth_item(list, 1);
        m_activeURI = webkit_back_forward_list_item_get_original_uri(item);
    }

    // Call go_forward even when can_go_forward returns FALSE to check nothing happens.
    webkit_web_view_go_forward(m_webView);
}

void WebViewTest::goToBackForwardListItem(WebKitBackForwardListItem* item)
{
    m_activeURI = webkit_back_forward_list_item_get_original_uri(item);
    webkit_web_view_go_to_back_forward_list_item(m_webView, item);
}

void WebViewTest::wait(double seconds)
{
    g_timeout_add_seconds(seconds, reinterpret_cast<GSourceFunc>(testLoadTimeoutFinishLoop), m_mainLoop);
    g_main_loop_run(m_mainLoop);
}

static void loadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent, WebViewTest* test)
{
    if (loadEvent != WEBKIT_LOAD_FINISHED)
        return;
    g_signal_handlers_disconnect_by_func(webView, reinterpret_cast<void*>(loadChanged), test);
    g_main_loop_quit(test->m_mainLoop);
}

void WebViewTest::waitUntilLoadFinished()
{
    g_signal_connect(m_webView, "load-changed", G_CALLBACK(loadChanged), this);
    g_main_loop_run(m_mainLoop);
}

static gboolean parentWindowMapped(GtkWidget* widget, GdkEvent*, WebViewTest* test)
{
    g_signal_handlers_disconnect_by_func(widget, reinterpret_cast<void*>(parentWindowMapped), test);
    g_main_loop_quit(test->m_mainLoop);

    return FALSE;
}

void WebViewTest::showInWindowAndWaitUntilMapped()
{
    g_assert(!m_parentWindow);
    m_parentWindow = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_container_add(GTK_CONTAINER(m_parentWindow), GTK_WIDGET(m_webView));
    gtk_widget_show(GTK_WIDGET(m_webView));

    g_signal_connect(m_parentWindow, "map-event", G_CALLBACK(parentWindowMapped), this);
    gtk_widget_show(m_parentWindow);
    g_main_loop_run(m_mainLoop);
}

void WebViewTest::mouseMoveTo(int x, int y, unsigned int mouseModifiers)
{
    g_assert(m_parentWindow);
    GtkWidget* viewWidget = GTK_WIDGET(m_webView);
    g_assert(gtk_widget_get_realized(viewWidget));

    GOwnPtr<GdkEvent> event(gdk_event_new(GDK_MOTION_NOTIFY));
    event->motion.x = x;
    event->motion.y = y;

    event->motion.time = GDK_CURRENT_TIME;
    event->motion.window = gtk_widget_get_window(viewWidget);
    g_object_ref(event->motion.window);
    event->button.device = gdk_device_manager_get_client_pointer(gdk_display_get_device_manager(gtk_widget_get_display(viewWidget)));
    event->motion.state = mouseModifiers;
    event->motion.axes = 0;

    int xRoot, yRoot;
    gdk_window_get_root_coords(gtk_widget_get_window(viewWidget), x, y, &xRoot, &yRoot);
    event->motion.x_root = xRoot;
    event->motion.y_root = yRoot;
    gtk_main_do_event(event.get());
}

