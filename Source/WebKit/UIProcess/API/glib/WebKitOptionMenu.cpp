/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "WebKitOptionMenu.h"

#include "WebKitOptionMenuItemPrivate.h"
#include "WebKitOptionMenuPrivate.h"
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

/**
 * WebKitOptionMenu:
 *
 * Represents the dropdown menu of a `select` element in a #WebKitWebView.
 *
 * When a select element in a #WebKitWebView needs to display a dropdown menu, the signal
 * #WebKitWebView::show-option-menu is emitted, providing a WebKitOptionMenu with the
 * #WebKitOptionMenuItem<!-- -->s that should be displayed.
 *
 * Since: 2.18
 */

struct _WebKitOptionMenuPrivate {
    Vector<WebKitOptionMenuItem> items;
    RefPtr<WebKitPopupMenu> popupMenu;
#if PLATFORM(GTK)
#if USE(GTK4)
    GRefPtr<GdkEvent> event;
#else
    GUniquePtr<GdkEvent> event;
#endif
#endif
};

enum {
    CLOSE,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitOptionMenu, webkit_option_menu, G_TYPE_OBJECT)

static void webkit_option_menu_class_init(WebKitOptionMenuClass* optionMenuClass)
{
    /**
     * WebKitOptionMenu::close:
     * @menu: the #WebKitOptionMenu on which the signal is emitted
     *
     * Emitted when closing a #WebKitOptionMenu is requested. This can happen
     * when the user explicitly calls webkit_option_menu_close() or when the
     * element is detached from the current page.
     *
     * Since: 2.18
     */
    signals[CLOSE] =
        g_signal_new("close",
            G_TYPE_FROM_CLASS(optionMenuClass),
            G_SIGNAL_RUN_LAST,
            0, nullptr, nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
}

WebKitOptionMenu* webkitOptionMenuCreate(WebKitPopupMenu& popupMenu, const Vector<WebPopupItem>& items, int32_t selectedIndex)
{
    auto* menu = WEBKIT_OPTION_MENU(g_object_new(WEBKIT_TYPE_OPTION_MENU, nullptr));
    menu->priv->popupMenu = &popupMenu;
    menu->priv->items.reserveInitialCapacity(items.size());
    for (const auto& item : items)
        menu->priv->items.uncheckedAppend(WebKitOptionMenuItem(item));
    if (selectedIndex >= 0) {
        ASSERT(static_cast<unsigned>(selectedIndex) < menu->priv->items.size());
        menu->priv->items[selectedIndex].isSelected = true;
    }
    return menu;
}

#if PLATFORM(GTK)
void webkitOptionMenuSetEvent(WebKitOptionMenu* menu, GdkEvent* event)
{
#if USE(GTK4)
    menu->priv->event = event;
#else
    menu->priv->event.reset(event ? gdk_event_copy(event) : nullptr);
#endif
}
#endif

/**
 * webkit_option_menu_get_n_items:
 * @menu: a #WebKitOptionMenu
 *
 * Gets the length of the @menu.
 *
 * Returns: the number of #WebKitOptionMenuItem<!-- -->s in @menu
 *
 * Since: 2.18
 */
guint webkit_option_menu_get_n_items(WebKitOptionMenu* menu)
{
    g_return_val_if_fail(WEBKIT_IS_OPTION_MENU(menu), 0);

    return menu->priv->items.size();
}

/**
 * webkit_option_menu_get_item:
 * @menu: a #WebKitOptionMenu
 * @index: the index of the item
 *
 * Returns the #WebKitOptionMenuItem at @index in @menu.
 *
 * Returns: (transfer none): a #WebKitOptionMenuItem of @menu.
 *
 * Since: 2.18
 */
WebKitOptionMenuItem* webkit_option_menu_get_item(WebKitOptionMenu* menu, guint index)
{
    g_return_val_if_fail(WEBKIT_IS_OPTION_MENU(menu), nullptr);
    g_return_val_if_fail(index < menu->priv->items.size(), nullptr);

    return &menu->priv->items[index];
}

/**
 * webkit_option_menu_select_item:
 * @menu: a #WebKitOptionMenu
 * @index: the index of the item
 *
 * Selects the #WebKitOptionMenuItem at @index in @menu.
 *
 * Selecting an item changes the
 * text shown by the combo button, but it doesn't change the value of the element. You need to
 * explicitly activate the item with webkit_option_menu_select_item() or close the menu with
 * webkit_option_menu_close() in which case the currently selected item will be activated.
 *
 * Since: 2.18
 */
void webkit_option_menu_select_item(WebKitOptionMenu* menu, guint index)
{
    g_return_if_fail(WEBKIT_IS_OPTION_MENU(menu));
    g_return_if_fail(index < menu->priv->items.size());

    menu->priv->popupMenu->selectItem(index);
}

/**
 * webkit_option_menu_activate_item:
 * @menu: a #WebKitOptionMenu
 * @index: the index of the item
 *
 * Activates the #WebKitOptionMenuItem at @index in @menu.
 *
 * Activating an item changes the value
 * of the element making the item the active one. You are expected to close the menu with
 * webkit_option_menu_close() after activating an item, calling this function again will have no
 * effect.
 *
 * Since: 2.18
 */
void webkit_option_menu_activate_item(WebKitOptionMenu* menu, guint index)
{
    g_return_if_fail(WEBKIT_IS_OPTION_MENU(menu));
    g_return_if_fail(index < menu->priv->items.size());

    menu->priv->popupMenu->activateItem(index);
}

/**
 * webkit_option_menu_close:
 * @menu: a #WebKitOptionMenu
 *
 * Request to close a #WebKitOptionMenu.
 *
 * This emits WebKitOptionMenu::close signal.
 * This function should always be called to notify WebKit that the associated
 * menu has been closed. If the menu is closed and neither webkit_option_menu_select_item()
 * nor webkit_option_menu_activate_item() have been called, the element value remains
 * unchanged.
 *
 * Since: 2.18
 */
void webkit_option_menu_close(WebKitOptionMenu* menu)
{
    g_return_if_fail(WEBKIT_IS_OPTION_MENU(menu));

    g_signal_emit(menu, signals[CLOSE], 0, nullptr);
}

#if PLATFORM(GTK)
/**
 * webkit_option_menu_get_event:
 * @menu: a #WebKitOptionMenu
 *
 * Gets the #GdkEvent that triggered the dropdown menu.
 * If @menu was not triggered by a user interaction, like a mouse click,
 * %NULL is returned.
 *
 * Returns: (transfer none): the menu event or %NULL.
 *
 * Since: 2.40
 */
GdkEvent* webkit_option_menu_get_event(WebKitOptionMenu* menu)
{
    g_return_val_if_fail(WEBKIT_IS_OPTION_MENU(menu), nullptr);

    return menu->priv->event.get();
}
#endif
