/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
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

#ifndef webkitwebviewcommon_h
#define webkitwebviewcommon_h

G_BEGIN_DECLS

WEBKIT_API GType
webkit_web_view_get_type (void);

WEBKIT_API GtkWidget *
webkit_web_view_new (void);

WEBKIT_API G_CONST_RETURN gchar *
webkit_web_view_get_title                       (WebKitWebView        *webView);

WEBKIT_API void
webkit_web_view_go_back                         (WebKitWebView        *webView);

WEBKIT_API void
webkit_web_view_go_forward                      (WebKitWebView        *webView);

WEBKIT_API void
webkit_web_view_load_uri                        (WebKitWebView        *webView,
                                                 const gchar          *uri);

G_END_DECLS

#endif
