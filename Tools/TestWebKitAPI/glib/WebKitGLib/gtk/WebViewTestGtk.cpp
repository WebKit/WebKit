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

#include "WebKitWebViewBaseInternal.h"
#include <WebCore/GUniquePtrGtk.h>
#include <WebCore/GtkVersioning.h>
#include <gtk/gtk.h>

void WebViewTest::platformDestroy()
{
    if (m_parentWindow)
        gtk_widget_destroy(m_parentWindow);
}

void WebViewTest::platformInitializeWebView()
{
    g_assert_true(WEBKIT_WEB_VIEW(m_webView));
    g_assert_true(g_object_is_floating(m_webView));
    g_object_ref_sink(m_webView);
}

void WebViewTest::quitMainLoopAfterProcessingPendingEvents()
{
    while (g_main_context_pending(nullptr))
        g_main_context_iteration(nullptr, TRUE);
    quitMainLoop();
}

void WebViewTest::resizeView(int width, int height)
{
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(m_webView), &allocation);
    if (width != -1)
        allocation.width = width;
    if (height != -1)
        allocation.height = height;
    gtk_widget_size_allocate(GTK_WIDGET(m_webView), &allocation);
}

void WebViewTest::hideView()
{
    gtk_widget_hide(GTK_WIDGET(m_webView));
}

void WebViewTest::showInWindow(int width, int height)
{
    g_assert_null(m_parentWindow);
    m_parentWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (width && height)
        gtk_window_set_default_size(GTK_WINDOW(m_parentWindow), width, height);
    gtk_container_add(GTK_CONTAINER(m_parentWindow), GTK_WIDGET(m_webView));
    gtk_widget_show(GTK_WIDGET(m_webView));
    gtk_widget_show(m_parentWindow);

    while (g_main_context_pending(nullptr))
        g_main_context_iteration(nullptr, TRUE);
}

void WebViewTest::mouseMoveTo(int x, int y, unsigned mouseModifiers)
{
    g_assert_nonnull(m_parentWindow);
    webkitWebViewBaseSynthesizeMouseEvent(WEBKIT_WEB_VIEW_BASE(m_webView), MouseEventType::Motion, 0, 0, x, y, mouseModifiers, 0);
}

void WebViewTest::clickMouseButton(int x, int y, unsigned button, unsigned mouseModifiers)
{
    webkitWebViewBaseSynthesizeMouseEvent(WEBKIT_WEB_VIEW_BASE(m_webView), MouseEventType::Press, button, 1 << (8 + button - 1), x, y, mouseModifiers, 1);
    webkitWebViewBaseSynthesizeMouseEvent(WEBKIT_WEB_VIEW_BASE(m_webView), MouseEventType::Release, button, 0, x, y, mouseModifiers, 0);
}

void WebViewTest::emitPopupMenuSignal()
{
    GtkWidget* viewWidget = GTK_WIDGET(m_webView);
    g_assert_true(gtk_widget_get_realized(viewWidget));

    gboolean handled;
    g_signal_emit_by_name(viewWidget, "popup-menu", &handled);
}

void WebViewTest::keyStroke(unsigned keyVal, unsigned keyModifiers)
{
    g_assert_nonnull(m_parentWindow);
    webkitWebViewBaseSynthesizeKeyEvent(WEBKIT_WEB_VIEW_BASE(m_webView), KeyEventType::Insert, keyVal, keyModifiers, ShouldTranslateKeyboardState::No);
}
