/*
 * Copyright (C) 2018 Igalia S.L.
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

#if !defined(__WEBKITDOM_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkitdom.h> can be included directly."
#endif

#ifndef WebKitDOMObject_h
#define WebKitDOMObject_h

#include <glib-object.h>
#include <wpe/WebKitDOMDefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_OBJECT            (webkit_dom_object_get_type())
#define WEBKIT_DOM_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_OBJECT, WebKitDOMObject))
#define WEBKIT_DOM_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_OBJECT, WebKitDOMObjectClass))
#define WEBKIT_DOM_IS_OBJECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_OBJECT))
#define WEBKIT_DOM_IS_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_OBJECT))
#define WEBKIT_DOM_OBJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_OBJECT, WebKitDOMObjectClass))

typedef struct _WebKitDOMObject WebKitDOMObject;
typedef struct _WebKitDOMObjectClass WebKitDOMObjectClass;

struct _WebKitDOMObject {
    GObject parentInstance;
};

struct _WebKitDOMObjectClass {
    GObjectClass parentClass;
};

WEBKIT_API GType
webkit_dom_object_get_type(void);

G_END_DECLS

#endif /* WebKitDOMObject_h */
