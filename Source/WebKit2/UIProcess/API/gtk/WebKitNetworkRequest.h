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

#ifndef WebKitNetworkRequest_h
#define WebKitNetworkRequest_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_NETWORK_REQUEST            (webkit_network_request_get_type())
#define WEBKIT_NETWORK_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_NETWORK_REQUEST, WebKitNetworkRequest))
#define WEBKIT_IS_NETWORK_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_NETWORK_REQUEST))
#define WEBKIT_NETWORK_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_NETWORK_REQUEST, WebKitNetworkRequestClass))
#define WEBKIT_IS_NETWORK_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_NETWORK_REQUEST))
#define WEBKIT_NETWORK_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_NETWORK_REQUEST, WebKitNetworkRequestClass))

typedef struct _WebKitNetworkRequest WebKitNetworkRequest;
typedef struct _WebKitNetworkRequestClass WebKitNetworkRequestClass;
typedef struct _WebKitNetworkRequestPrivate WebKitNetworkRequestPrivate;

struct _WebKitNetworkRequest {
    GObject parent;

    /*< private >*/
    WebKitNetworkRequestPrivate *priv;
};

struct _WebKitNetworkRequestClass {
    GObjectClass parent_class;

};

WEBKIT_API GType
webkit_network_request_get_type (void);

WEBKIT_API WebKitNetworkRequest *
webkit_network_request_new      (const gchar          *uri);

WEBKIT_API const gchar *
webkit_network_request_get_uri  (WebKitNetworkRequest *request);

G_END_DECLS

#endif
