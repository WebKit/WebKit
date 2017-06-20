/*
 * Copyright (C) 2013 Igalia S.L.
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

#if !defined(__WEBKIT_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit.h> can be included directly."
#endif

#ifndef WebKitNotificationPermissionRequest_h
#define WebKitNotificationPermissionRequest_h

#include <glib-object.h>
#include <wpe/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_NOTIFICATION_PERMISSION_REQUEST            (webkit_notification_permission_request_get_type())
#define WEBKIT_NOTIFICATION_PERMISSION_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_NOTIFICATION_PERMISSION_REQUEST, WebKitNotificationPermissionRequest))
#define WEBKIT_NOTIFICATION_PERMISSION_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_NOTIFICATION_PERMISSION_REQUEST, WebKitNotificationPermissionRequestClass))
#define WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_NOTIFICATION_PERMISSION_REQUEST))
#define WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_NOTIFICATION_PERMISSION_REQUEST))
#define WEBKIT_NOTIFICATION_PERMISSION_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_NOTIFICATION_PERMISSION_REQUEST, WebKitNotificationPermissionRequestClass))

typedef struct _WebKitNotificationPermissionRequest        WebKitNotificationPermissionRequest;
typedef struct _WebKitNotificationPermissionRequestClass   WebKitNotificationPermissionRequestClass;
typedef struct _WebKitNotificationPermissionRequestPrivate WebKitNotificationPermissionRequestPrivate;

struct _WebKitNotificationPermissionRequest {
    GObject parent;

    /*< private >*/
    WebKitNotificationPermissionRequestPrivate *priv;
};

struct _WebKitNotificationPermissionRequestClass {
    GObjectClass parent_class;
};

WEBKIT_API GType
webkit_notification_permission_request_get_type (void);

G_END_DECLS

#endif
