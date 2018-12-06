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

#include <WebKit/WKImageCairo.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKView.h>
#include <WebKit/WKViewPrivate.h>
#include <gtk/gtk.h>
#include <wtf/Assertions.h>

namespace WTR {

PlatformWebView::PlatformWebView(WKPageConfigurationRef configuration, const TestOptions& options)
    : m_view(WKViewCreate(configuration))
    , m_window(gtk_window_new(GTK_WINDOW_POPUP))
    , m_windowIsKey(true)
    , m_options(options)
{
    gtk_container_add(GTK_CONTAINER(m_window), GTK_WIDGET(m_view));

    GtkAllocation size = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(m_view), &size);
    gtk_window_resize(GTK_WINDOW(m_window), 800, 600);
    gtk_widget_show_all(m_window);

    while (gtk_events_pending())
        gtk_main_iteration();
}

PlatformWebView::~PlatformWebView()
{
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
        m_otherWindow = gtk_window_new(GTK_WINDOW_POPUP);
        gtk_widget_show_all(m_otherWindow);
        gtk_window_present(GTK_WINDOW(m_otherWindow));
    }

    while (gtk_events_pending())
        gtk_main_iteration();
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
    GtkAllocation geometry;
    gdk_window_get_geometry(gtk_widget_get_window(GTK_WIDGET(m_window)),
                            &geometry.x, &geometry.y, &geometry.width, &geometry.height);

    WKRect frame;
    frame.origin.x = geometry.x;
    frame.origin.y = geometry.y;
    frame.size.width = geometry.width;
    frame.size.height = geometry.height;
    return frame;
}

void PlatformWebView::setWindowFrame(WKRect frame, WebViewSizingMode)
{
    gdk_window_move_resize(gtk_widget_get_window(GTK_WIDGET(m_window)),
        frame.origin.x, frame.origin.y, frame.size.width, frame.size.height);
    GtkAllocation size = { 0, 0, static_cast<int>(frame.size.width), static_cast<int>(frame.size.height) };
    gtk_widget_size_allocate(GTK_WIDGET(m_view), &size);

    while (gtk_events_pending())
        gtk_main_iteration();
}

void PlatformWebView::addChromeInputField()
{
}

void PlatformWebView::removeChromeInputField()
{
}

void PlatformWebView::addToWindow()
{
}

void PlatformWebView::removeFromWindow()
{
}

void PlatformWebView::makeWebViewFirstResponder()
{
}

void PlatformWebView::changeWindowScaleIfNeeded(float)
{
}

cairo_surface_t* PlatformWebView::windowSnapshotImage()
{
    int width = gtk_widget_get_allocated_width(GTK_WIDGET(m_view));
    int height = gtk_widget_get_allocated_height(GTK_WIDGET(m_view));

    while (gtk_events_pending())
        gtk_main_iteration();

    cairo_surface_t* imageSurface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);

    cairo_t* context = cairo_create(imageSurface);
    gtk_widget_draw(GTK_WIDGET(m_view), context);
    cairo_destroy(context);

    return imageSurface;
}

void PlatformWebView::didInitializeClients()
{
}

void PlatformWebView::dismissAllPopupMenus()
{
    // gtk_menu_popdown doesn't modify the GList of attached menus, so it should
    // be safe to walk this list while calling it.
    GList* attachedMenusList = gtk_menu_get_for_attach_widget(GTK_WIDGET(m_view));
    g_list_foreach(attachedMenusList, [] (void* data, void*) {
        ASSERT(data);
        gtk_menu_popdown(GTK_MENU(data));
    }, nullptr);
}

void PlatformWebView::setNavigationGesturesEnabled(bool)
{
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

