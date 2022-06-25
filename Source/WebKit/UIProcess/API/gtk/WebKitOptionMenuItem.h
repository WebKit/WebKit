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

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitOptionMenuItem_h
#define WebKitOptionMenuItem_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_OPTION_MENU_ITEM (webkit_option_menu_item_get_type())

typedef struct _WebKitOptionMenuItem WebKitOptionMenuItem;

WEBKIT_API GType
webkit_option_menu_item_get_type       (void);

WEBKIT_API WebKitOptionMenuItem *
webkit_option_menu_item_copy           (WebKitOptionMenuItem *item);

WEBKIT_API void
webkit_option_menu_item_free           (WebKitOptionMenuItem *item);

WEBKIT_API const gchar *
webkit_option_menu_item_get_label      (WebKitOptionMenuItem *item);

WEBKIT_API const gchar *
webkit_option_menu_item_get_tooltip    (WebKitOptionMenuItem *item);

WEBKIT_API gboolean
webkit_option_menu_item_is_group_label (WebKitOptionMenuItem *item);

WEBKIT_API gboolean
webkit_option_menu_item_is_group_child (WebKitOptionMenuItem *item);

WEBKIT_API gboolean
webkit_option_menu_item_is_enabled     (WebKitOptionMenuItem *item);

WEBKIT_API gboolean
webkit_option_menu_item_is_selected    (WebKitOptionMenuItem *item);

G_END_DECLS

#endif
