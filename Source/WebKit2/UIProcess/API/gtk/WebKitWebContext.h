/*
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef WebKitWebContext_h
#define WebKitWebContext_h

#include <glib-object.h>
#include <WebKit2/WKBase.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_CONTEXT            (webkit_web_context_get_type())
#define WEBKIT_WEB_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_CONTEXT, WebKitWebContext))
#define WEBKIT_WEB_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_CONTEXT, WebKitWebContextClass))
#define WEBKIT_IS_WEB_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_CONTEXT))
#define WEBKIT_IS_WEB_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_CONTEXT))
#define WEBKIT_WEB_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_CONTEXT, WebKitWebContextClass))

typedef struct _WebKitWebContext        WebKitWebContext;
typedef struct _WebKitWebContextClass   WebKitWebContextClass;
typedef struct _WebKitWebContextPrivate WebKitWebContextPrivate;

struct _WebKitWebContext {
    GObject parent;

    /*< private >*/
    WebKitWebContextPrivate *priv;
};

struct _WebKitWebContextClass {
    GObjectClass parent;

    /* Padding for future expansion */
    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WK_EXPORT GType
webkit_web_context_get_type    (void);

WK_EXPORT WebKitWebContext *
webkit_web_context_get_default (void);

G_END_DECLS

#endif
