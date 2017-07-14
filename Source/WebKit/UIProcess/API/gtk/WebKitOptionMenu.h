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

#ifndef WebKitOptionMenu_h
#define WebKitOptionMenu_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>
#include <webkit2/WebKitOptionMenuItem.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_OPTION_MENU            (webkit_option_menu_get_type())
#define WEBKIT_OPTION_MENU(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_OPTION_MENU, WebKitOptionMenu))
#define WEBKIT_IS_OPTION_MENU(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_OPTION_MENU))
#define WEBKIT_OPTION_MENU_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_OPTION_MENU, WebKitOptionMenuClass))
#define WEBKIT_IS_OPTION_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_OPTION_MENU))
#define WEBKIT_OPTION_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_OPTION_MENU, WebKitOptionMenuClass))

typedef struct _WebKitOptionMenu        WebKitOptionMenu;
typedef struct _WebKitOptionMenuClass   WebKitOptionMenuClass;
typedef struct _WebKitOptionMenuPrivate WebKitOptionMenuPrivate;

struct _WebKitOptionMenu {
    GObject parent;

    WebKitOptionMenuPrivate *priv;
};

struct _WebKitOptionMenuClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_option_menu_get_type       (void);

WEBKIT_API guint
webkit_option_menu_get_n_items    (WebKitOptionMenu *menu);

WEBKIT_API WebKitOptionMenuItem *
webkit_option_menu_get_item       (WebKitOptionMenu *menu,
                                   guint             index);

WEBKIT_API void
webkit_option_menu_select_item    (WebKitOptionMenu *menu,
                                   guint             index);

WEBKIT_API void
webkit_option_menu_activate_item  (WebKitOptionMenu *menu,
                                   guint             index);

WEBKIT_API void
webkit_option_menu_close          (WebKitOptionMenu *menu);

G_END_DECLS

#endif
