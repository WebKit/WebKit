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

#include <WebKit/GtkVersioning.h>
#include <gtk/gtk.h>
#include <webkit/WebKitWebViewBaseInternal.h>

void WebViewTest::platformDestroy()
{
#if USE(GTK4)
    if (m_parentWindow)
        gtk_window_destroy(GTK_WINDOW(m_parentWindow));
#else
    if (m_parentWindow)
        gtk_widget_destroy(m_parentWindow);
#endif
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
    gtk_widget_get_allocation(GTK_WIDGET(m_webView.get()), &allocation);
    if (width != -1)
        allocation.width = width;
    if (height != -1)
        allocation.height = height;
    gtk_widget_size_allocate(GTK_WIDGET(m_webView.get()), &allocation);
}

void WebViewTest::hideView()
{
    gtk_widget_hide(GTK_WIDGET(m_webView.get()));
}

void WebViewTest::showInWindow(int width, int height)
{
    g_assert_null(m_parentWindow);
#if USE(GTK4)
    m_parentWindow = gtk_window_new();
    gtk_window_set_child(GTK_WINDOW(m_parentWindow), GTK_WIDGET(m_webView.get()));
#else
    m_parentWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_add(GTK_CONTAINER(m_parentWindow), GTK_WIDGET(m_webView.get()));
    gtk_widget_show(GTK_WIDGET(m_webView.get()));
#endif

    if (width && height)
        gtk_window_set_default_size(GTK_WINDOW(m_parentWindow), width, height);

    gtk_widget_show(m_parentWindow);

    while (g_main_context_pending(nullptr))
        g_main_context_iteration(nullptr, TRUE);
}

static unsigned testModifiersToGDK(const OptionSet<WebViewTest::Modifiers> modifiers)
{
    unsigned gdkModifiers = 0;
    if (modifiers.contains(WebViewTest::Modifiers::Control))
        gdkModifiers |= GDK_CONTROL_MASK;
    if (modifiers.contains(WebViewTest::Modifiers::Shift))
        gdkModifiers |= GDK_SHIFT_MASK;
    if (modifiers.contains(WebViewTest::Modifiers::Alt))
        gdkModifiers |= GDK_MOD1_MASK;
    if (modifiers.contains(WebViewTest::Modifiers::Meta))
        gdkModifiers |= GDK_META_MASK;
    return gdkModifiers;
}

static unsigned testMouseButtonToGDK(WebViewTest::MouseButton button)
{
    switch (button) {
    case WebViewTest::MouseButton::Primary:
        return GDK_BUTTON_PRIMARY;
    case WebViewTest::MouseButton::Middle:
        return GDK_BUTTON_MIDDLE;
    case WebViewTest::MouseButton::Secondary:
        return GDK_BUTTON_SECONDARY;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void WebViewTest::mouseMoveTo(int x, int y, OptionSet<Modifiers> mouseModifiers)
{
    g_assert_nonnull(m_parentWindow);
    webkitWebViewBaseSynthesizeMouseEvent(WEBKIT_WEB_VIEW_BASE(m_webView.get()), MouseEventType::Motion, 0, 0, x, y, testModifiersToGDK(mouseModifiers), 0);
}

void WebViewTest::clickMouseButton(int x, int y, MouseButton button, OptionSet<Modifiers> mouseModifiers)
{
    auto gdkModifiers = testModifiersToGDK(mouseModifiers);
    auto gdkButton = testMouseButtonToGDK(button);
    webkitWebViewBaseSynthesizeMouseEvent(WEBKIT_WEB_VIEW_BASE(m_webView.get()), MouseEventType::Press, gdkButton, 1 << (8 + gdkButton - 1), x, y, gdkModifiers, 1);
    webkitWebViewBaseSynthesizeMouseEvent(WEBKIT_WEB_VIEW_BASE(m_webView.get()), MouseEventType::Release, gdkButton, 0, x, y, gdkModifiers, 0);
}

void WebViewTest::emitPopupMenuSignal()
{
    GtkWidget* viewWidget = GTK_WIDGET(m_webView.get());
    g_assert_true(gtk_widget_get_realized(viewWidget));

    gboolean handled;
    g_signal_emit_by_name(viewWidget, "popup-menu", &handled);
}

void WebViewTest::keyStroke(unsigned keyVal, OptionSet<Modifiers> keyModifiers)
{
    g_assert_nonnull(m_parentWindow);
    webkitWebViewBaseSynthesizeKeyEvent(WEBKIT_WEB_VIEW_BASE(m_webView.get()), KeyEventType::Insert, keyVal, testModifiersToGDK(keyModifiers), ShouldTranslateKeyboardState::No);
}
