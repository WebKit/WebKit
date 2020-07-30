/*
 * Copyright (C) 2012 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformWebView.h"

#include "WebKitWebViewBaseInternal.h"
#include <WebCore/GUniquePtrGtk.h>
#include <WebCore/GtkVersioning.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKView.h>
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

namespace TestWebKitAPI {

PlatformWebView::PlatformWebView(WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());
    WKPageConfigurationSetContext(configuration.get(), contextRef);
    WKPageConfigurationSetPageGroup(configuration.get(), pageGroupRef);

    initialize(configuration.get());
}

PlatformWebView::PlatformWebView(WKPageConfigurationRef configuration)
{
    initialize(configuration);
}

PlatformWebView::PlatformWebView(WKPageRef relatedPage)
{
    WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());
    WKPageConfigurationSetContext(configuration.get(), WKPageGetContext(relatedPage));
    WKPageConfigurationSetPageGroup(configuration.get(), WKPageGetPageGroup(relatedPage));
    WKPageConfigurationSetRelatedPage(configuration.get(), relatedPage);

    initialize(configuration.get());
}

PlatformWebView::~PlatformWebView()
{
#if USE(GTK4)
    gtk_window_destroy(GTK_WINDOW(m_window));
#else
    gtk_widget_destroy(m_window);
#endif
}

void PlatformWebView::initialize(WKPageConfigurationRef configuration)
{
    m_view = WKViewCreate(configuration);
#if USE(GTK4)
    m_window = gtk_window_new();
    gtk_window_set_child(GTK_WINDOW(m_window), GTK_WIDGET(m_view));
#else
    m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_add(GTK_CONTAINER(m_window), GTK_WIDGET(m_view));
    gtk_widget_show(GTK_WIDGET(m_view));
#endif
    gtk_widget_show(m_window);
}

WKPageRef PlatformWebView::page() const
{
    return WKViewGetPage(m_view);
}

void PlatformWebView::resizeTo(unsigned width, unsigned height)
{
    gtk_window_resize(GTK_WINDOW(m_window), width, height);
}

static inline WebKitWebViewBase* toWebKitGLibAPI(PlatformWKView view)
{
    return const_cast<WebKitWebViewBase*>(reinterpret_cast<const WebKitWebViewBase*>(view));
}

void PlatformWebView::simulateSpacebarKeyPress()
{
    GtkWidget* viewWidget = GTK_WIDGET(m_view);
    if (!gtk_widget_get_realized(viewWidget))
        gtk_widget_show(m_window);
    webkitWebViewBaseSynthesizeKeyEvent(toWebKitGLibAPI(m_view), KeyEventType::Insert, GDK_KEY_KP_Space, 0, ShouldTranslateKeyboardState::No);
}

void PlatformWebView::simulateAltKeyPress()
{
    GtkWidget* viewWidget = GTK_WIDGET(m_view);
    if (!gtk_widget_get_realized(viewWidget))
        gtk_widget_show(m_window);
    webkitWebViewBaseSynthesizeKeyEvent(toWebKitGLibAPI(m_view), KeyEventType::Insert, GDK_KEY_Alt_L, 0, ShouldTranslateKeyboardState::No);
}

void PlatformWebView::simulateRightClick(unsigned x, unsigned y)
{
    GtkWidget* viewWidget = GTK_WIDGET(m_view);
    if (!gtk_widget_get_realized(viewWidget))
        gtk_widget_show(m_window);
    webkitWebViewBaseSynthesizeMouseEvent(toWebKitGLibAPI(m_view), MouseEventType::Press, 3, 0, x, y, 0, 1);
    webkitWebViewBaseSynthesizeMouseEvent(toWebKitGLibAPI(m_view), MouseEventType::Release, 3, 0, x, y, 0, 0);
}

void PlatformWebView::simulateMouseMove(unsigned x, unsigned y, WKEventModifiers)
{
    webkitWebViewBaseSynthesizeMouseEvent(toWebKitGLibAPI(m_view), MouseEventType::Motion, 0, 0, x, y, 0, 0);
}

} // namespace TestWebKitAPI
