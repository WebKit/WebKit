/*
 * Copyright (C) 2025 Igalia S.L.
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

#pragma once

#if ENABLE(CONTEXT_MENUS)
#include "WebKitDefines.h"
#include <gio/gio.h>

#if PLATFORM(GTK) && !USE(GTK4)
#include <gtk/gtk.h>
#endif

namespace WebKit {
class WebContextMenuItemData;
class WebPageProxy;
}

#define WEBKIT_TYPE_CONTEXT_MENU_GACTION            (webkit_context_menu_gaction_get_type())
#if !ENABLE(2022_GLIB_API)
#define WEBKIT_CONTEXT_MENU_GACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_CONTEXT_MENU_GACTION, WebKitContextMenuGAction))
#define WEBKIT_IS_CONTEXT_MENU_GACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_CONTEXT_MENU_GACTION))
#define WEBKIT_CONTEXT_MENU_GACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_CONTEXT_MENU_GACTION, WebKitContextMenuGActionClass))
#define WEBKIT_IS_CONTEXT_MENU_GACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_CONTEXT_MENU_GACTION))
#define WEBKIT_CONTEXT_MENU_GACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_CONTEXT_MENU_GACTION, WebKitContextMenuGActionClass))

struct _WebKitContextMenuGActionClass {
    GObjectClass parentClass;
};
#endif

WEBKIT_DECLARE_FINAL_TYPE(WebKitContextMenuGAction, webkit_context_menu_gaction, WEBKIT, CONTEXT_MENU_GACTION, GObject)

GAction* webkitContextMenuGActionNew(const char*, const WebKit::WebContextMenuItemData&);
void webkitContextMenuGActionSetPage(WebKitContextMenuGAction*, WebKit::WebPageProxy*);

#if PLATFORM(GTK) && !USE(GTK4)
void webkitContextMenuGActionSetGtkAction(WebKitContextMenuGAction*, GtkAction*);
#endif

#endif // ENABLE(CONTEXT_MENUS)
