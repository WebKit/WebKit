/*
 * Copyright (C) 2020 Igalia S.L
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

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitWebsiteDataAccessPermissionRequest_h
#define WebKitWebsiteDataAccessPermissionRequest_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST            (webkit_website_data_access_permission_request_get_type())
#define WEBKIT_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST, WebKitWebsiteDataAccessPermissionRequest))
#define WEBKIT_IS_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST))
#define WEBKIT_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST, WebKitWebsiteDataAccessPermissionRequestClass))
#define WEBKIT_IS_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST))
#define WEBKIT_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST, WebKitWebsiteDataAccessPermissionRequestClass))

typedef struct _WebKitWebsiteDataAccessPermissionRequest        WebKitWebsiteDataAccessPermissionRequest;
typedef struct _WebKitWebsiteDataAccessPermissionRequestClass   WebKitWebsiteDataAccessPermissionRequestClass;
typedef struct _WebKitWebsiteDataAccessPermissionRequestPrivate WebKitWebsiteDataAccessPermissionRequestPrivate;

struct _WebKitWebsiteDataAccessPermissionRequest {
    GObject parent;

    /*< private >*/
    WebKitWebsiteDataAccessPermissionRequestPrivate *priv;
};

struct _WebKitWebsiteDataAccessPermissionRequestClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_website_data_access_permission_request_get_type              (void);

WEBKIT_API const char *
webkit_website_data_access_permission_request_get_requesting_domain (WebKitWebsiteDataAccessPermissionRequest *request);

WEBKIT_API const char *
webkit_website_data_access_permission_request_get_current_domain    (WebKitWebsiteDataAccessPermissionRequest *request);

G_END_DECLS

#endif
