/*
 * Copyright (C) 2022 Igalia S.L.
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
#include "WebKitWebView.h"

#include <gtk/gtk.h>

#if !USE(GTK4)
guint createContextMenuSignal(WebKitWebViewClass* webViewClass)
{
    /**
     * WebKitWebView::context-menu:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @context_menu: the proposed #WebKitContextMenu
     * @event: the #GdkEvent that triggered the context menu. Deprecated 2.40.
     * @hit_test_result: a #WebKitHitTestResult
     *
     * Emitted when a context menu is about to be displayed to give the application
     * a chance to customize the proposed menu, prevent the menu from being displayed,
     * or build its own context menu.
     * <itemizedlist>
     * <listitem><para>
     *  To customize the proposed menu you can use webkit_context_menu_prepend(),
     *  webkit_context_menu_append() or webkit_context_menu_insert() to add new
     *  #WebKitContextMenuItem<!-- -->s to @context_menu, webkit_context_menu_move_item()
     *  to reorder existing items, or webkit_context_menu_remove() to remove an
     *  existing item. The signal handler should return %FALSE, and the menu represented
     *  by @context_menu will be shown.
     * </para></listitem>
     * <listitem><para>
     *  To prevent the menu from being displayed you can just connect to this signal
     *  and return %TRUE so that the proposed menu will not be shown.
     * </para></listitem>
     * <listitem><para>
     *  To build your own menu, you can remove all items from the proposed menu with
     *  webkit_context_menu_remove_all(), add your own items and return %FALSE so
     *  that the menu will be shown. You can also ignore the proposed #WebKitContextMenu,
     *  build your own #GtkMenu and return %TRUE to prevent the proposed menu from being shown.
     * </para></listitem>
     * <listitem><para>
     *  If you just want the default menu to be shown always, simply don't connect to this
     *  signal because showing the proposed context menu is the default behaviour.
     * </para></listitem>
     * </itemizedlist>
     *
     * The @event parameter is now deprecated. Use webkit_context_menu_get_event() to get the
     * #GdkEvent that triggered the context menu.
     *
     * If the signal handler returns %FALSE the context menu represented by @context_menu
     * will be shown, if it return %TRUE the context menu will not be shown.
     *
     * The proposed #WebKitContextMenu passed in @context_menu argument is only valid
     * during the signal emission.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    return g_signal_new(
        "context-menu",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, context_menu),
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN,
        3,
        WEBKIT_TYPE_CONTEXT_MENU,
        GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE,
        WEBKIT_TYPE_HIT_TEST_RESULT);
}
#endif
