/*
 * Copyright (C) 2018 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined(__WEBKIT_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit.h> can be included directly."
#endif

#ifndef WebKitDeviceInfoPermissionRequest_h
#define WebKitDeviceInfoPermissionRequest_h

#include <glib-object.h>
#include <wpe/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_DEVICE_INFO_PERMISSION_REQUEST            (webkit_device_info_permission_request_get_type())
#define WEBKIT_DEVICE_INFO_PERMISSION_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_DEVICE_INFO_PERMISSION_REQUEST, WebKitDeviceInfoPermissionRequest))
#define WEBKIT_DEVICE_INFO_PERMISSION_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_DEVICE_INFO_PERMISSION_REQUEST, WebKitDeviceInfoPermissionRequestClass))
#define WEBKIT_IS_DEVICE_INFO_PERMISSION_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_DEVICE_INFO_PERMISSION_REQUEST))
#define WEBKIT_IS_DEVICE_INFO_PERMISSION_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_DEVICE_INFO_PERMISSION_REQUEST))
#define WEBKIT_DEVICE_INFO_PERMISSION_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_DEVICE_INFO_PERMISSION_REQUEST, WebKitDeviceInfoPermissionRequestClass))

typedef struct _WebKitDeviceInfoPermissionRequest        WebKitDeviceInfoPermissionRequest;
typedef struct _WebKitDeviceInfoPermissionRequestClass   WebKitDeviceInfoPermissionRequestClass;
typedef struct _WebKitDeviceInfoPermissionRequestPrivate WebKitDeviceInfoPermissionRequestPrivate;

struct _WebKitDeviceInfoPermissionRequest {
    GObject parent;

    /*< private >*/
    WebKitDeviceInfoPermissionRequestPrivate *priv;
};

struct _WebKitDeviceInfoPermissionRequestClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_device_info_permission_request_get_type    (void);

G_END_DECLS

#endif
