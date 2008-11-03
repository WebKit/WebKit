/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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

#ifndef WEBKIT_WEB_FRAME_H
#define WEBKIT_WEB_FRAME_H

#include <glib-object.h>
#include <JavaScriptCore/JSBase.h>

#include <webkit/webkitdefines.h>
#include <webkit/webkitnetworkrequest.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_FRAME            (webkit_web_frame_get_type())
#define WEBKIT_WEB_FRAME(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_FRAME, WebKitWebFrame))
#define WEBKIT_WEB_FRAME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_FRAME, WebKitWebFrameClass))
#define WEBKIT_IS_WEB_FRAME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_FRAME))
#define WEBKIT_IS_WEB_FRAME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_FRAME))
#define WEBKIT_WEB_FRAME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_FRAME, WebKitWebFrameClass))

typedef struct _WebKitWebFramePrivate WebKitWebFramePrivate;

struct _WebKitWebFrame {
    GObject parent_instance;

    WebKitWebFramePrivate *priv;
};

struct _WebKitWebFrameClass {
    GObjectClass parent_class;

    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
    void (*_webkit_reserved4) (void);
    void (*_webkit_reserved5) (void);
    void (*_webkit_reserved6) (void);
};

WEBKIT_API GType
webkit_web_frame_get_type           (void);

#ifndef WEBKIT_DISABLE_DEPRECATED
WEBKIT_API WebKitWebFrame *
webkit_web_frame_new                (WebKitWebView        *web_view);
#endif

WEBKIT_API WebKitWebView *
webkit_web_frame_get_web_view       (WebKitWebFrame       *frame);

WEBKIT_API G_CONST_RETURN gchar *
webkit_web_frame_get_name           (WebKitWebFrame       *frame);

WEBKIT_API G_CONST_RETURN gchar *
webkit_web_frame_get_title          (WebKitWebFrame       *frame);

WEBKIT_API G_CONST_RETURN gchar *
webkit_web_frame_get_uri            (WebKitWebFrame       *frame);

WEBKIT_API WebKitWebFrame*
webkit_web_frame_get_parent         (WebKitWebFrame       *frame);

WEBKIT_API void
webkit_web_frame_load_request       (WebKitWebFrame       *frame,
                                     WebKitNetworkRequest *request);

WEBKIT_API void
webkit_web_frame_stop_loading       (WebKitWebFrame       *frame);

WEBKIT_API void
webkit_web_frame_reload             (WebKitWebFrame       *frame);

WEBKIT_API WebKitWebFrame *
webkit_web_frame_find_frame         (WebKitWebFrame       *frame,
                                     const gchar          *name);

WEBKIT_API JSGlobalContextRef
webkit_web_frame_get_global_context (WebKitWebFrame       *frame);

G_END_DECLS

#endif
