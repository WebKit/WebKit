/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 University of Szeged. All rights reserved.
 * Copyright (C) 2010 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformWebView.h"

#include <WebCore/GtkVersioning.h>
#include <WebKit/WKImageCairo.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKView.h>
#include <WebKit/WKViewPrivate.h>
#include <gtk/gtk.h>
#include <wtf/Assertions.h>
#include <wtf/glib/GRefPtr.h>

namespace WTR {

PlatformWebView::PlatformWebView(WKPageConfigurationRef configuration, const TestOptions& options)
    : m_view(WKViewCreate(configuration))
#if USE(GTK4)
    , m_window(gtk_window_new())
#else
    , m_window(gtk_window_new(GTK_WINDOW_POPUP))
#endif
    , m_windowIsKey(true)
    , m_options(options)
{
#if USE(GTK4)
    gtk_window_set_default_size(GTK_WINDOW(m_window), 800, 600);
    gtk_window_set_child(GTK_WINDOW(m_window), GTK_WIDGET(m_view));
#else
    gtk_container_add(GTK_CONTAINER(m_window), GTK_WIDGET(m_view));
    gtk_widget_show(GTK_WIDGET(m_view));

    GtkAllocation size = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(m_view), &size);
    gtk_window_resize(GTK_WINDOW(m_window), 800, 600);
#endif
    gtk_widget_show(m_window);

    while (g_main_context_pending(nullptr))
        g_main_context_iteration(nullptr, TRUE);
}

PlatformWebView::~PlatformWebView()
{
#if USE(GTK4)
    if (gtk_window_get_child(GTK_WINDOW(m_window)) != GTK_WIDGET(m_view))
        g_object_unref(GTK_WIDGET(m_view));
#else
    if (gtk_bin_get_child(GTK_BIN(m_window)) != GTK_WIDGET(m_view))
        g_object_unref(GTK_WIDGET(m_view));
#endif

    gtk_widget_destroy(m_window);
    if (m_otherWindow)
        gtk_widget_destroy(m_otherWindow);
}

void PlatformWebView::setWindowIsKey(bool isKey)
{
    if (m_windowIsKey == isKey)
        return;

    m_windowIsKey = isKey;

    if (isKey) {
        if (m_otherWindow) {
            gtk_widget_destroy(m_otherWindow);
            m_otherWindow = nullptr;
        }
        gtk_window_present(GTK_WINDOW(m_window));
    } else {
        ASSERT(!m_otherWindow);
#if USE(GTK4)
        m_otherWindow = gtk_window_new();
#else
        m_otherWindow = gtk_window_new(GTK_WINDOW_POPUP);
        gtk_widget_show_all(m_otherWindow);
#endif
        gtk_window_present(GTK_WINDOW(m_otherWindow));
    }

    while (g_main_context_pending(nullptr))
        g_main_context_iteration(nullptr, TRUE);
}

void PlatformWebView::resizeTo(unsigned width, unsigned height, WebViewSizingMode sizingMode)
{
    WKRect frame = windowFrame();
    frame.size.width = width;
    frame.size.height = height;
    setWindowFrame(frame, sizingMode);
}

WKPageRef PlatformWebView::page()
{
    return WKViewGetPage(m_view);
}

void PlatformWebView::focus()
{
    WKViewSetFocus(m_view, true);
    setWindowIsKey(true);
}

WKRect PlatformWebView::windowFrame()
{
    GtkAllocation geometry = { 0, 0, 0, 0 };
#if USE(GTK4)
    gtk_window_get_size(GTK_WINDOW(m_window), &geometry.width, &geometry.height);
#else
    gdk_window_get_geometry(gtk_widget_get_window(GTK_WIDGET(m_window)),
                            &geometry.x, &geometry.y, &geometry.width, &geometry.height);
#endif

    WKRect frame;
    frame.origin.x = geometry.x;
    frame.origin.y = geometry.y;
    frame.size.width = geometry.width;
    frame.size.height = geometry.height;
    return frame;
}

void PlatformWebView::setWindowFrame(WKRect frame, WebViewSizingMode)
{
#if USE(GTK4)
    gtk_window_resize(GTK_WINDOW(m_window), frame.size.width, frame.size.height);
#else
    gdk_window_move_resize(gtk_widget_get_window(GTK_WIDGET(m_window)),
        frame.origin.x, frame.origin.y, frame.size.width, frame.size.height);
    GtkAllocation size = { 0, 0, static_cast<int>(frame.size.width), static_cast<int>(frame.size.height) };
    gtk_widget_size_allocate(GTK_WIDGET(m_view), &size);
#endif

    while (g_main_context_pending(nullptr))
        g_main_context_iteration(nullptr, TRUE);
}

void PlatformWebView::addChromeInputField()
{
}

void PlatformWebView::removeChromeInputField()
{
}

void PlatformWebView::addToWindow()
{
#if USE(GTK4)
    if (gtk_window_get_child(GTK_WINDOW(m_window)) == GTK_WIDGET(m_view))
        return;

    gtk_window_set_child(GTK_WINDOW(m_window), GTK_WIDGET(m_view));
#else
    if (gtk_bin_get_child(GTK_BIN(m_window)) == GTK_WIDGET(m_view))
        return;

    gtk_container_add(GTK_CONTAINER(m_window), GTK_WIDGET(m_view));
#endif
    g_object_unref(GTK_WIDGET(m_view));
}

void PlatformWebView::removeFromWindow()
{
#if USE(GTK4)
    if (gtk_window_get_child(GTK_WINDOW(m_window)) == GTK_WIDGET(m_view)) {
        g_object_ref(GTK_WIDGET(m_view));
        gtk_window_set_child(GTK_WINDOW(m_window), nullptr);
    }
#else
    if (gtk_bin_get_child(GTK_BIN(m_window)) == GTK_WIDGET(m_view)) {
        g_object_ref(GTK_WIDGET(m_view));
        gtk_container_remove(GTK_CONTAINER(m_window), GTK_WIDGET(m_view));
    }
#endif
}

void PlatformWebView::makeWebViewFirstResponder()
{
}

void PlatformWebView::changeWindowScaleIfNeeded(float)
{
}

cairo_surface_t* PlatformWebView::windowSnapshotImage()
{
#if USE(GTK4)
    int width = gtk_widget_get_width(GTK_WIDGET(m_view));
    int height = gtk_widget_get_height(GTK_WIDGET(m_view));
#else
    int width = gtk_widget_get_allocated_width(GTK_WIDGET(m_view));
    int height = gtk_widget_get_allocated_height(GTK_WIDGET(m_view));
#endif

    while (g_main_context_pending(nullptr))
        g_main_context_iteration(nullptr, TRUE);

    cairo_surface_t* imageSurface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
    cairo_t* context = cairo_create(imageSurface);

#if USE(GTK4)
    GRefPtr<GdkPaintable> paintable = adoptGRef(gtk_widget_paintable_new(GTK_WIDGET(m_view)));
    auto* snapshot = gtk_snapshot_new();
    gdk_paintable_snapshot(paintable.get(), snapshot, width, height);
    if (auto* node = gtk_snapshot_free_to_node(snapshot)) {
        gsk_render_node_draw(node, context);
        gsk_render_node_unref(node);
    }
#else
    gtk_widget_draw(GTK_WIDGET(m_view), context);
#endif

    cairo_destroy(context);

    return imageSurface;
}

void PlatformWebView::didInitializeClients()
{
}

void PlatformWebView::dismissAllPopupMenus()
{
#if !USE(GTK4)
    // gtk_menu_popdown doesn't modify the GList of attached menus, so it should
    // be safe to walk this list while calling it.
    GList* attachedMenusList = gtk_menu_get_for_attach_widget(GTK_WIDGET(m_view));
    g_list_foreach(attachedMenusList, [] (void* data, void*) {
        ASSERT(data);
        gtk_menu_popdown(GTK_MENU(data));
    }, nullptr);
#endif
}

void PlatformWebView::setNavigationGesturesEnabled(bool enabled)
{
    WKViewSetEnableBackForwardNavigationGesture(platformView(), enabled);
}

bool PlatformWebView::drawsBackground() const
{
    return false;
}

void PlatformWebView::setDrawsBackground(bool)
{
}

void PlatformWebView::setEditable(bool)
{
}

} // namespace WTR

