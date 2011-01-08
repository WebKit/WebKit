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

#ifndef WEBKIT_SOUP_REQUEST_H
#define WEBKIT_SOUP_REQUEST_H 1

#include <libsoup/soup.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_SOUP_REQUEST            (webkit_soup_request_get_type ())
#define WEBKIT_SOUP_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_TYPE_SOUP_REQUEST, WebKitSoupRequest))
#define WEBKIT_SOUP_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_SOUP_REQUEST, WebKitSoupRequestClass))
#define WEBKIT_IS_SOUP_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_TYPE_SOUP_REQUEST))
#define WEBKIT_IS_SOUP_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), WEBKIT_TYPE_SOUP_REQUEST))
#define WEBKIT_SOUP_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), WEBKIT_TYPE_SOUP_REQUEST, WebKitSoupRequestClass))

typedef struct _WebKitSoupRequest WebKitSoupRequest;
typedef struct _WebKitSoupRequestPrivate WebKitSoupRequestPrivate;
typedef struct _WebKitSoupRequestClass WebKitSoupRequestClass;

struct _WebKitSoupRequest {
	GObject parent;

	WebKitSoupRequestPrivate *priv;
};

struct _WebKitSoupRequestClass {
	GObjectClass parent;

	gboolean (*check_uri)(WebKitSoupRequest          *req_base,
			      SoupURI              *uri,
			      GError              **error);

	GInputStream * (*send)(WebKitSoupRequest          *request,
			       GCancellable         *cancellable,
			       GError              **error);
	void (*send_async)(WebKitSoupRequest          *request,
			   GCancellable         *cancellable,
			   GAsyncReadyCallback callback,
			   gpointer user_data);
	GInputStream * (*send_finish)(WebKitSoupRequest          *request,
				      GAsyncResult         *result,
				      GError              **error);

	goffset (*get_content_length)(WebKitSoupRequest   *request);
	const char *   (*get_content_type)(WebKitSoupRequest   *request);
};

GType webkit_soup_request_get_type (void);

#define WEBKIT_SOUP_REQUEST_URI     "uri"
#define WEBKIT_SOUP_REQUEST_SESSION "session"

GInputStream *webkit_soup_request_send (WebKitSoupRequest          *request,
					GCancellable         *cancellable,
					GError              **error);
void          webkit_soup_request_send_async (WebKitSoupRequest          *request,
					      GCancellable         *cancellable,
					      GAsyncReadyCallback callback,
					      gpointer user_data);
GInputStream *webkit_soup_request_send_finish (WebKitSoupRequest          *request,
					       GAsyncResult         *result,
					       GError              **error);

SoupURI      *webkit_soup_request_get_uri (WebKitSoupRequest          *request);
SoupSession  *webkit_soup_request_get_session (WebKitSoupRequest          *request);

goffset       webkit_soup_request_get_content_length (WebKitSoupRequest          *request);
const char   *webkit_soup_request_get_content_type (WebKitSoupRequest          *request);

/* Used by WebKitSoupRequestFile and WebKitSoupRequestData. Ideally
 * should be located in some util file but I'll place it here as it
 * will be removed with libsoup 2.32.1 that will ship fixed versions
 * of soup_uri_decode/normalize
 */
gchar *webkit_soup_request_uri_decoded_copy (const char *part, int length);

G_END_DECLS

#endif /* WEBKIT_SOUP_REQUEST_H */
