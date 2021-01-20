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

#if !defined(__WEBKIT_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit.h> can be included directly."
#endif

#ifndef WebKitWebViewBackend_h
#define WebKitWebViewBackend_h

#include <glib-object.h>
#include <wpe/WebKitDefines.h>
#include <wpe/wpe.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_VIEW_BACKEND (webkit_web_view_backend_get_type())

typedef struct _WebKitWebViewBackend WebKitWebViewBackend;

WEBKIT_API GType
webkit_web_view_backend_get_type        (void);

WEBKIT_API WebKitWebViewBackend *
webkit_web_view_backend_new             (struct wpe_view_backend *backend,
                                         GDestroyNotify           notify,
                                         gpointer                 user_data);
WEBKIT_API struct wpe_view_backend *
webkit_web_view_backend_get_wpe_backend (WebKitWebViewBackend    *view_backend);

G_END_DECLS

#endif /* WebKitWebViewBackend_h */
