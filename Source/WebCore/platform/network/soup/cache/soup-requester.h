/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2010 Igalia S.L.
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

#ifndef WEBKIT_SOUP_REQUESTER_H
#define WEBKIT_SOUP_REQUESTER_H 1

#include "soup-request.h"
#include <libsoup/soup.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_SOUP_REQUESTER            (webkit_soup_requester_get_type ())
#define WEBKIT_SOUP_REQUESTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_TYPE_SOUP_REQUESTER, WebKitSoupRequester))
#define WEBKIT_SOUP_REQUESTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_SOUP_REQUESTER, WebKitSoupRequesterClass))
#define WEBKIT_IS_SOUP_REQUESTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_TYPE_SOUP_REQUESTER))
#define WEBKIT_IS_SOUP_REQUESTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), WEBKIT_TYPE_SOUP_REQUESTER))
#define WEBKIT_SOUP_REQUESTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), WEBKIT_TYPE_SOUP_REQUESTER, WebKitSoupRequesterClass))

#define WEBKIT_SOUP_ERROR webkit_soup_error_quark ()

typedef enum {
	WEBKIT_SOUP_ERROR_BAD_URI,
	WEBKIT_SOUP_ERROR_UNSUPPORTED_URI_SCHEME
} WebKitSoupError;

typedef struct _WebKitSoupRequester WebKitSoupRequester;
typedef struct _WebKitSoupRequesterPrivate WebKitSoupRequesterPrivate;

struct _WebKitSoupRequester {
	GObject parent;

	WebKitSoupRequesterPrivate *priv;
};

typedef struct {
	GObjectClass parent_class;
} WebKitSoupRequesterClass;

GType webkit_soup_requester_get_type (void);

WebKitSoupRequester *webkit_soup_requester_new (void);

GQuark webkit_soup_error_quark (void);

WebKitSoupRequest *webkit_soup_requester_request (WebKitSoupRequester *requester,
						  const char *uriString,
						  SoupSession *session,
						  GError **error);

WebKitSoupRequest *webkit_soup_requester_request_uri (WebKitSoupRequester *requester,
						      SoupURI *uri,
						      SoupSession *session,
						      GError **error);

void webkit_soup_requester_add_protocol (WebKitSoupRequester *requester,
					 const char  *scheme,
					 GType request_type);

void webkit_soup_requester_remove_protocol (WebKitSoupRequester *requester,
					    const char  *scheme);

G_END_DECLS

#endif /* WEBKIT_SOUP_REQUESTER_H */
