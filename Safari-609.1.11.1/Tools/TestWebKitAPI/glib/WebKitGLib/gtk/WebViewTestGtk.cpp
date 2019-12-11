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

#include <WebCore/GUniquePtrGtk.h>
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
    while (gtk_events_pending())
        gtk_main_iteration();
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

static gboolean parentWindowMapped(GtkWidget* widget, GdkEvent*, WebViewTest* test)
{
    g_signal_handlers_disconnect_by_func(widget, reinterpret_cast<void*>(parentWindowMapped), test);
    g_main_loop_quit(test->m_mainLoop);

    return FALSE;
}

void WebViewTest::showInWindow(GtkWindowType windowType)
{
    g_assert_null(m_parentWindow);
    m_parentWindow = gtk_window_new(windowType);
    gtk_container_add(GTK_CONTAINER(m_parentWindow), GTK_WIDGET(m_webView));
    gtk_widget_show(GTK_WIDGET(m_webView));
    gtk_widget_show(m_parentWindow);
}

void WebViewTest::showInWindowAndWaitUntilMapped(GtkWindowType windowType, int width, int height)
{
    g_assert_null(m_parentWindow);
    m_parentWindow = gtk_window_new(windowType);
    if (width && height)
        gtk_window_resize(GTK_WINDOW(m_parentWindow), width, height);
    gtk_container_add(GTK_CONTAINER(m_parentWindow), GTK_WIDGET(m_webView));
    gtk_widget_show(GTK_WIDGET(m_webView));

    g_signal_connect(m_parentWindow, "map-event", G_CALLBACK(parentWindowMapped), this);
    gtk_widget_show(m_parentWindow);
    g_main_loop_run(m_mainLoop);
}

void WebViewTest::mouseMoveTo(int x, int y, unsigned mouseModifiers)
{
    g_assert_nonnull(m_parentWindow);
    GtkWidget* viewWidget = GTK_WIDGET(m_webView);
    g_assert_true(gtk_widget_get_realized(viewWidget));

    GUniquePtr<GdkEvent> event(gdk_event_new(GDK_MOTION_NOTIFY));
    event->motion.x = x;
    event->motion.y = y;

    event->motion.time = GDK_CURRENT_TIME;
    event->motion.window = gtk_widget_get_window(viewWidget);
    g_object_ref(event->motion.window);
    event->motion.device = gdk_device_manager_get_client_pointer(gdk_display_get_device_manager(gtk_widget_get_display(viewWidget)));
    event->motion.state = mouseModifiers;
    event->motion.axes = 0;

    int xRoot, yRoot;
    gdk_window_get_root_coords(gtk_widget_get_window(viewWidget), x, y, &xRoot, &yRoot);
    event->motion.x_root = xRoot;
    event->motion.y_root = yRoot;
    gtk_main_do_event(event.get());
}

void WebViewTest::clickMouseButton(int x, int y, unsigned button, unsigned mouseModifiers)
{
    doMouseButtonEvent(GDK_BUTTON_PRESS, x, y, button, mouseModifiers);
    doMouseButtonEvent(GDK_BUTTON_RELEASE, x, y, button, mouseModifiers);
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
    GtkWidget* viewWidget = GTK_WIDGET(m_webView);
    g_assert_true(gtk_widget_get_realized(viewWidget));

    GUniquePtr<GdkEvent> event(gdk_event_new(GDK_KEY_PRESS));
    event->key.keyval = keyVal;

    event->key.time = GDK_CURRENT_TIME;
    event->key.window = gtk_widget_get_window(viewWidget);
    g_object_ref(event->key.window);
    gdk_event_set_device(event.get(), gdk_device_manager_get_client_pointer(gdk_display_get_device_manager(gtk_widget_get_display(viewWidget))));
    event->key.state = keyModifiers;

    // When synthesizing an event, an invalid hardware_keycode value can cause it to be badly processed by GTK+.
    GUniqueOutPtr<GdkKeymapKey> keys;
    int keysCount;
    if (gdk_keymap_get_entries_for_keyval(gdk_keymap_get_default(), keyVal, &keys.outPtr(), &keysCount) && keysCount)
        event->key.hardware_keycode = keys.get()[0].keycode;

    gtk_main_do_event(event.get());
    event->key.type = GDK_KEY_RELEASE;
    gtk_main_do_event(event.get());
}

void WebViewTest::doMouseButtonEvent(GdkEventType eventType, int x, int y, unsigned button, unsigned mouseModifiers)
{
    g_assert_nonnull(m_parentWindow);
    GtkWidget* viewWidget = GTK_WIDGET(m_webView);
    g_assert_true(gtk_widget_get_realized(viewWidget));

    GUniquePtr<GdkEvent> event(gdk_event_new(eventType));
    event->button.window = gtk_widget_get_window(viewWidget);
    g_object_ref(event->button.window);

    event->button.time = GDK_CURRENT_TIME;
    event->button.x = x;
    event->button.y = y;
    event->button.axes = 0;
    event->button.state = mouseModifiers;
    event->button.button = button;

    event->button.device = gdk_device_manager_get_client_pointer(gdk_display_get_device_manager(gtk_widget_get_display(viewWidget)));

    int xRoot, yRoot;
    gdk_window_get_root_coords(gtk_widget_get_window(viewWidget), x, y, &xRoot, &yRoot);
    event->button.x_root = xRoot;
    event->button.y_root = yRoot;
    gtk_main_do_event(event.get());
}
