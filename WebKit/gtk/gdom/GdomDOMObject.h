/*
 * Copyright (C) 2008, 2009 Luke Kenneth Casson Leighton <lkcl@lkcl.net>
 * Copyright (C) 2008 Martin Soto <soto@freedesktop.org>
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Apple Inc.
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


#ifndef GdomDOMObject_h
#define GdomDOMObject_h

#include <glib-object.h>
#include <webkit/webkitdefines.h>

G_BEGIN_DECLS

typedef struct _GdomDOMObject GdomDOMObject;
typedef struct _GdomDOMObjectClass GdomDOMObjectClass;

#define GDOM_TYPE_DOM_OBJECT            (gdom_dom_object_get_type())
#define GDOM_DOM_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GDOM_TYPE_DOM_OBJECT, GdomDOMObject))
#define GDOM_DOM_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  GDOM_TYPE_DOM_OBJECT, GdomDOMObjectClass))
#define GDOM_IS_DOM_OBJECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GDOM_TYPE_DOM_OBJECT))
#define GDOM_IS_DOM_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  GDOM_TYPE_DOM_OBJECT))
#define GDOM_DOM_OBJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  GDOM_TYPE_DOM_OBJECT, GdomDOMObjectClass))

typedef struct _GdomDOMObjectPrivate GdomDOMObjectPrivate;

struct _GdomDOMObject {
    GObject parent_instance;

    GdomDOMObjectPrivate *priv;
};

struct _GdomDOMObjectClass {
    GObjectClass parent_class;
};

WEBKIT_API GType
gdom_dom_object_get_type (void);

G_END_DECLS

#endif /* GdomDOMObject_h */

