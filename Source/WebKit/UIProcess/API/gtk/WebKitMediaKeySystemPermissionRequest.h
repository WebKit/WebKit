/*
 * Copyright (C) 2021 Igalia S.L.
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

#ifndef WebKitMediaKeySystemPermissionRequest_h
#define WebKitMediaKeySystemPermissionRequest_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST            (webkit_media_key_system_permission_request_get_type())
#define WEBKIT_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST, WebKitMediaKeySystemPermissionRequest))
#define WEBKIT_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST, WebKitMediaKeySystemPermissionRequestClass))
#define WEBKIT_IS_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST))
#define WEBKIT_IS_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST))
#define WEBKIT_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST, WebKitMediaKeySystemPermissionRequestClass))

typedef struct _WebKitMediaKeySystemPermissionRequest        WebKitMediaKeySystemPermissionRequest;
typedef struct _WebKitMediaKeySystemPermissionRequestClass   WebKitMediaKeySystemPermissionRequestClass;
typedef struct _WebKitMediaKeySystemPermissionRequestPrivate WebKitMediaKeySystemPermissionRequestPrivate;

struct _WebKitMediaKeySystemPermissionRequest {
    GObject parent;

    /*< private >*/
    WebKitMediaKeySystemPermissionRequestPrivate *priv;
};

struct _WebKitMediaKeySystemPermissionRequestClass {
    GObjectClass parent_class;

    /*< private >*/
    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_media_key_system_permission_request_get_type (void);

WEBKIT_API const gchar *
webkit_media_key_system_permission_get_name (WebKitMediaKeySystemPermissionRequest *request);

G_END_DECLS

#endif
