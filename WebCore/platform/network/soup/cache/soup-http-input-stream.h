/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, 2007, 2009 Red Hat, Inc.
 * Copyright (C) 2010 Igalia, S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __WEBKIT_SOUP_HTTP_INPUT_STREAM_H__
#define __WEBKIT_SOUP_HTTP_INPUT_STREAM_H__

#include <gio/gio.h>
#include <libsoup/soup-types.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_SOUP_HTTP_INPUT_STREAM         (webkit_soup_http_input_stream_get_type ())
#define WEBKIT_SOUP_HTTP_INPUT_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), WEBKIT_TYPE_SOUP_HTTP_INPUT_STREAM, WebKitSoupHTTPInputStream))
#define WEBKIT_SOUP_HTTP_INPUT_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), WEBKIT_TYPE_SOUP_HTTP_INPUT_STREAM, WebKitSoupHTTPInputStreamClass))
#define WEBKIT_IS_SOUP_HTTP_INPUT_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), WEBKIT_TYPE_SOUP_HTTP_INPUT_STREAM))
#define WEBKIT_IS_SOUP_HTTP_INPUT_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), WEBKIT_TYPE_SOUP_HTTP_INPUT_STREAM))
#define WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), WEBKIT_TYPE_SOUP_HTTP_INPUT_STREAM, WebKitSoupHTTPInputStreamClass))

typedef struct WebKitSoupHTTPInputStream WebKitSoupHTTPInputStream;
typedef struct WebKitSoupHTTPInputStreamClass WebKitSoupHTTPInputStreamClass;

struct WebKitSoupHTTPInputStream {
	GInputStream parent;
};

struct WebKitSoupHTTPInputStreamClass {
	GInputStreamClass parent_class;

	/* Padding for future expansion */
	void (*_g_reserved1)(void);
	void (*_g_reserved2)(void);
	void (*_g_reserved3)(void);
	void (*_g_reserved4)(void);
	void (*_g_reserved5)(void);
};

GType webkit_soup_http_input_stream_get_type (void) G_GNUC_CONST;

WebKitSoupHTTPInputStream *webkit_soup_http_input_stream_new (SoupSession         *session,
							      SoupMessage         *msg);

gboolean             webkit_soup_http_input_stream_send (WebKitSoupHTTPInputStream *httpstream,
							 GCancellable        *cancellable,
							 GError             **error);

void                 webkit_soup_http_input_stream_send_async (WebKitSoupHTTPInputStream *httpstream,
							       int io_priority,
							       GCancellable        *cancellable,
							       GAsyncReadyCallback callback,
							       gpointer user_data);
gboolean             webkit_soup_http_input_stream_send_finish (WebKitSoupHTTPInputStream *httpstream,
								GAsyncResult        *result,
								GError             **error);

SoupMessage         *webkit_soup_http_input_stream_get_message (WebKitSoupHTTPInputStream *httpstream);

G_END_DECLS

#endif /* __WEBKIT_SOUP_HTTP_INPUT_STREAM_H__ */
