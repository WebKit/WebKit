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

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitWebLoaderClient_h
#define WebKitWebLoaderClient_h

#include <WebKit2/WKBase.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_LOADER_CLIENT            (webkit_web_loader_client_get_type())
#define WEBKIT_WEB_LOADER_CLIENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_LOADER_CLIENT, WebKitWebLoaderClient))
#define WEBKIT_WEB_LOADER_CLIENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_LOADER_CLIENT, WebKitWebLoaderClientClass))
#define WEBKIT_IS_WEB_LOADER_CLIENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_LOADER_CLIENT))
#define WEBKIT_IS_WEB_LOADER_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_LOADER_CLIENT))
#define WEBKIT_WEB_LOADER_CLIENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_LOADER_CLIENT, WebKitWebLoaderClientClass))

typedef struct _WebKitWebLoaderClient        WebKitWebLoaderClient;
typedef struct _WebKitWebLoaderClientClass   WebKitWebLoaderClientClass;
typedef struct _WebKitWebLoaderClientPrivate WebKitWebLoaderClientPrivate;

struct _WebKitWebLoaderClient {
    GObject parent;

    WebKitWebLoaderClientPrivate *priv;
};

struct _WebKitWebLoaderClientClass {
    GObjectClass parent_class;

    /* virtual table */
    gboolean (* provisional_load_started)                  (WebKitWebLoaderClient *loader_client);
    gboolean (* provisional_load_received_server_redirect) (WebKitWebLoaderClient *loader_client);
    gboolean (* provisional_load_failed)                   (WebKitWebLoaderClient *loader_client,
                                                            const gchar           *failing_uri,
                                                            GError                *error);
    gboolean (* load_committed)                            (WebKitWebLoaderClient *loader_client);
    gboolean (* load_finished)                             (WebKitWebLoaderClient *loader_client);
    gboolean (* load_failed)                               (WebKitWebLoaderClient *loader_client,
                                                            const gchar           *failing_uri,
                                                            GError                *error);
};

WK_EXPORT GType
webkit_web_loader_client_get_type (void);

G_END_DECLS

#endif
