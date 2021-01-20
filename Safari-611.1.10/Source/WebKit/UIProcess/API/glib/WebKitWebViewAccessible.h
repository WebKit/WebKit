/*
 * Copyright (C) 2012, 2019 Igalia S.L.
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

#if ENABLE(ACCESSIBILITY)

#include <atk/atk.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_VIEW_ACCESSIBLE              (webkit_web_view_accessible_get_type())
#define WEBKIT_WEB_VIEW_ACCESSIBLE(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), WEBKIT_TYPE_WEB_VIEW_ACCESSIBLE, WebKitWebViewAccessible))
#define WEBKIT_WEB_VIEW_ACCESSIBLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_WEB_VIEW_ACCESSIBLE, WebKitWebViewAccessibleClass))
#define WEBKIT_IS_WEB_VIEW_ACCESSIBLE(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), WEBKIT_TYPE_WEB_VIEW_ACCESSIBLE))
#define WEBKIT_IS_WEB_VIEW_ACCESSIBLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_WEB_VIEW_ACCESSIBLE))
#define WEBKIT_WEB_VIEW_ACCESSIBLE_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), WEBKIT_TYPE_WEB_VIEW_ACCESSIBLE, WebKitWebViewAccessibleClass))

typedef struct _WebKitWebViewAccessible WebKitWebViewAccessible;
typedef struct _WebKitWebViewAccessibleClass WebKitWebViewAccessibleClass;
typedef struct _WebKitWebViewAccessiblePrivate WebKitWebViewAccessiblePrivate;

struct _WebKitWebViewAccessible {
    AtkSocket parent;
    /*< private >*/
    WebKitWebViewAccessiblePrivate* priv;
};

struct _WebKitWebViewAccessibleClass {
    AtkSocketClass parentClass;
};

GType webkit_web_view_accessible_get_type();

WebKitWebViewAccessible* webkitWebViewAccessibleNew(gpointer);
void webkitWebViewAccessibleSetWebView(WebKitWebViewAccessible*, gpointer);

G_END_DECLS

#endif // ENABLE(ACCESSIBILITY)
