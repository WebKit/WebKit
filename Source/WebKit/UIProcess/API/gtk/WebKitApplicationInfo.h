/*
 * Copyright (C) 2017 Igalia S.L.
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

#ifndef WebKitApplicationInfo_h
#define WebKitApplicationInfo_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_APPLICATION_INFO (webkit_application_info_get_type())

typedef struct _WebKitApplicationInfo WebKitApplicationInfo;


WEBKIT_API GType
webkit_application_info_get_type    (void);

WEBKIT_API WebKitApplicationInfo *
webkit_application_info_new         (void);

WEBKIT_API WebKitApplicationInfo *
webkit_application_info_ref         (WebKitApplicationInfo *info);

WEBKIT_API void
webkit_application_info_unref       (WebKitApplicationInfo *info);

WEBKIT_API void
webkit_application_info_set_name    (WebKitApplicationInfo *info,
                                     const gchar           *name);
WEBKIT_API const gchar *
webkit_application_info_get_name    (WebKitApplicationInfo *info);

WEBKIT_API void
webkit_application_info_set_version (WebKitApplicationInfo *info,
                                     guint64                major,
                                     guint64                minor,
                                     guint64                micro);

WEBKIT_API void
webkit_application_info_get_version (WebKitApplicationInfo *info,
                                     guint64               *major,
                                     guint64               *minor,
                                     guint64               *micro);

G_END_DECLS

#endif
