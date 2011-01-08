/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Red Hat, Inc.
 * Copyright (C) 2010 Igalia, S.L.
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

#ifndef WEBKIT_SOUP_REQUEST_HTTP_H
#define WEBKIT_SOUP_REQUEST_HTTP_H 1

#include "soup-request.h"

G_BEGIN_DECLS

#define WEBKIT_TYPE_SOUP_REQUEST_HTTP            (webkit_soup_request_http_get_type ())
#define WEBKIT_SOUP_REQUEST_HTTP(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), WEBKIT_TYPE_SOUP_REQUEST_HTTP, WebKitSoupRequestHTTP))
#define WEBKIT_SOUP_REQUEST_HTTP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_SOUP_REQUEST_HTTP, WebKitSoupRequestHTTPClass))
#define WEBKIT_IS_SOUP_REQUEST_HTTP(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), WEBKIT_TYPE_SOUP_REQUEST_HTTP))
#define WEBKIT_IS_SOUP_REQUEST_HTTP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), WEBKIT_TYPE_SOUP_REQUEST_HTTP))
#define WEBKIT_SOUP_REQUEST_HTTP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), WEBKIT_TYPE_SOUP_REQUEST_HTTP, WebKitSoupRequestHTTPClass))

typedef struct _WebKitSoupRequestHTTPPrivate WebKitSoupRequestHTTPPrivate;

typedef struct {
	WebKitSoupRequest parent;

	WebKitSoupRequestHTTPPrivate *priv;
} WebKitSoupRequestHTTP;

typedef struct {
	WebKitSoupRequestClass parent;
} WebKitSoupRequestHTTPClass;

GType webkit_soup_request_http_get_type (void);

SoupMessage  *webkit_soup_request_http_get_message (WebKitSoupRequestHTTP *http);

G_END_DECLS

#endif /* WEBKIT_SOUP_REQUEST_HTTP_H */
