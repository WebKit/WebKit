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
#include "WebKitOptionMenuItem.h"

#include "WebKitOptionMenuItemPrivate.h"

using namespace WebKit;

/**
 * SECTION: WebKitOptionMenuItem
 * @Short_description: One item of the #WebKitOptionMenu
 * @Title: WebKitOptionMenuItem
 *
 * The #WebKitOptionMenu is composed of WebKitOptionMenuItem<!-- -->s.
 * A WebKitOptionMenuItem always has a label and can contain a tooltip text.
 * You can use the WebKitOptionMenuItem of a #WebKitOptionMenu to build your
 * own menus.
 *
 * Since: 2.18
 */

G_DEFINE_BOXED_TYPE(WebKitOptionMenuItem, webkit_option_menu_item, webkit_option_menu_item_copy, webkit_option_menu_item_free)

/**
 * webkit_option_menu_item_copy:
 * @item: a #WebKitOptionMenuItem
 *
 * Make a copy of the #WebKitOptionMenuItem.
 *
 * Returns: (transfer full): A copy of passed in #WebKitOptionMenuItem
 *
 * Since: 2.18
 */
WebKitOptionMenuItem* webkit_option_menu_item_copy(WebKitOptionMenuItem* item)
{
    g_return_val_if_fail(item, nullptr);

    auto* copyItem = static_cast<WebKitOptionMenuItem*>(fastMalloc(sizeof(WebKitOptionMenuItem)));
    new (copyItem) WebKitOptionMenuItem(item);
    return copyItem;
}

/**
 * webkit_option_menu_item_free:
 * @item: A #WebKitOptionMenuItem
 *
 * Free the #WebKitOptionMenuItem.
 *
 * Since: 2.18
 */
void webkit_option_menu_item_free(WebKitOptionMenuItem* item)
{
    g_return_if_fail(item);

    item->~WebKitOptionMenuItem();
    fastFree(item);
}

/**
 * webkit_option_menu_item_get_label:
 * @item: a #WebKitOptionMenuItem
 *
 * Get the label of a #WebKitOptionMenuItem.
 *
 * Returns: The label of @item.
 *
 * Since: 2.18
 */
const gchar* webkit_option_menu_item_get_label(WebKitOptionMenuItem* item)
{
    g_return_val_if_fail(item, nullptr);

    return item->label.data();
}

/**
 * webkit_option_menu_item_get_tooltip:
 * @item: a #WebKitOptionMenuItem
 *
 * Get the tooltip of a #WebKitOptionMenuItem.
 *
 * Returns: The tooltip of @item, or %NULL.
 *
 * Since: 2.18
 */
const gchar* webkit_option_menu_item_get_tooltip(WebKitOptionMenuItem* item)
{
    g_return_val_if_fail(item, nullptr);

    return item->tooltip.isNull() ? nullptr : item->tooltip.data();
}

/**
 * webkit_option_menu_item_is_group_label:
 * @item: a #WebKitOptionMenuItem
 *
 * Whether a #WebKitOptionMenuItem is a group label.
 *
 * Returns: %TRUE if the @item is a group label or %FALSE otherwise.
 *
 * Since: 2.18
 */
gboolean webkit_option_menu_item_is_group_label(WebKitOptionMenuItem* item)
{
    g_return_val_if_fail(item, FALSE);

    return item->isGroupLabel;
}

/**
 * webkit_option_menu_item_is_group_child:
 * @item: a #WebKitOptionMenuItem
 *
 * Whether a #WebKitOptionMenuItem is a group child.
 *
 * Returns: %TRUE if the @item is a group child or %FALSE otherwise.
 *
 * Since: 2.18
 */
gboolean webkit_option_menu_item_is_group_child(WebKitOptionMenuItem* item)
{
    g_return_val_if_fail(item, FALSE);

    return item->isGroupChild;
}

/**
 * webkit_option_menu_item_is_enabled:
 * @item: a #WebKitOptionMenuItem
 *
 * Whether a #WebKitOptionMenuItem is enabled.
 *
 * Returns: %TRUE if the @item is enabled or %FALSE otherwise.
 *
 * Since: 2.18
 */
gboolean webkit_option_menu_item_is_enabled(WebKitOptionMenuItem* item)
{
    g_return_val_if_fail(item, FALSE);

    return item->isEnabled;
}

/**
 * webkit_option_menu_item_is_selected:
 * @item: a #WebKitOptionMenuItem
 *
 * Whether a #WebKitOptionMenuItem is the currently selected one.
 *
 * Returns: %TRUE if the @item is selected or %FALSE otherwise.
 *
 * Since: 2.18
 */
gboolean webkit_option_menu_item_is_selected(WebKitOptionMenuItem* item)
{
    g_return_val_if_fail(item, FALSE);

    return item->isSelected;
}
