/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#if !defined(__WEBKIT_WEB_EXTENSION_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit-web-extension.h> can be included directly."
#endif

#ifndef WebKitScriptWorld_h
#define WebKitScriptWorld_h

#include <glib-object.h>
#include <wpe/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_SCRIPT_WORLD            (webkit_script_world_get_type())
#define WEBKIT_SCRIPT_WORLD(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_SCRIPT_WORLD, WebKitScriptWorld))
#define WEBKIT_IS_SCRIPT_WORLD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_SCRIPT_WORLD))
#define WEBKIT_SCRIPT_WORLD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_SCRIPT_WORLD, WebKitScriptWorldClass))
#define WEBKIT_IS_SCRIPT_WORLD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_SCRIPT_WORLD))
#define WEBKIT_SCRIPT_WORLD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_SCRIPT_WORLD, WebKitScriptWorldClass))

typedef struct _WebKitScriptWorld        WebKitScriptWorld;
typedef struct _WebKitScriptWorldClass   WebKitScriptWorldClass;
typedef struct _WebKitScriptWorldPrivate WebKitScriptWorldPrivate;

struct _WebKitScriptWorld {
    GObject parent;

    WebKitScriptWorldPrivate *priv;
};

struct _WebKitScriptWorldClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_script_world_get_type      (void);

WEBKIT_API WebKitScriptWorld *
webkit_script_world_get_default   (void);

WEBKIT_API WebKitScriptWorld *
webkit_script_world_new           (void);

WEBKIT_API WebKitScriptWorld *
webkit_script_world_new_with_name (const char        *name);

WEBKIT_API const char *
webkit_script_world_get_name      (WebKitScriptWorld *world);

G_END_DECLS

#endif
