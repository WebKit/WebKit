/*
 * Copyright (C) 2013 Samsung Electronics Inc. All rights reserved.
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

#ifndef WebKitCertificateInfo_h
#define WebKitCertificateInfo_h

#include <gio/gio.h>
#include <glib-object.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_CERTIFICATE_INFO (webkit_certificate_info_get_type())

typedef struct _WebKitCertificateInfo WebKitCertificateInfo;

WEBKIT_API GType
webkit_certificate_info_get_type            (void);

WEBKIT_API WebKitCertificateInfo *
webkit_certificate_info_copy                (WebKitCertificateInfo *info);

WEBKIT_API void
webkit_certificate_info_free                (WebKitCertificateInfo *info);

WEBKIT_API GTlsCertificate *
webkit_certificate_info_get_tls_certificate (WebKitCertificateInfo *info);

WEBKIT_API GTlsCertificateFlags
webkit_certificate_info_get_tls_errors      (WebKitCertificateInfo *info);

G_END_DECLS

#endif
