/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-requester.c:
 *
 * Copyright (C) 2010, Igalia S.L.
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

#include "config.h"
#include "soup-requester.h"

#include "soup-request-data.h"
#include "soup-request-file.h"
#include "soup-request-http.h"
#include <glib/gi18n.h>
#include <libsoup/soup.h>

struct _WebKitSoupRequesterPrivate {
	GHashTable *request_types;
};

#define WEBKIT_SOUP_REQUESTER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), WEBKIT_TYPE_SOUP_REQUESTER, WebKitSoupRequesterPrivate))

G_DEFINE_TYPE (WebKitSoupRequester, webkit_soup_requester, G_TYPE_OBJECT)

static void webkit_soup_requester_init (WebKitSoupRequester *requester)
{
	requester->priv = WEBKIT_SOUP_REQUESTER_GET_PRIVATE (requester);

	requester->priv->request_types = 0;
}

static void finalize (GObject *object)
{
	WebKitSoupRequester *requester = WEBKIT_SOUP_REQUESTER (object);

	if (requester->priv->request_types)
		g_hash_table_destroy (requester->priv->request_types);

	G_OBJECT_CLASS (webkit_soup_requester_parent_class)->finalize (object);
}

static void webkit_soup_requester_class_init (WebKitSoupRequesterClass *requester_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (requester_class);

	g_type_class_add_private (requester_class, sizeof (WebKitSoupRequesterPrivate));

	/* virtual method override */
	object_class->finalize = finalize;
}

static void init_request_types (WebKitSoupRequesterPrivate *priv)
{
	if (priv->request_types)
		return;

	priv->request_types = g_hash_table_new_full (soup_str_case_hash,
						     soup_str_case_equal,
						     g_free, 0);
	g_hash_table_insert (priv->request_types, g_strdup ("file"),
			     GSIZE_TO_POINTER (WEBKIT_TYPE_SOUP_REQUEST_FILE));
	g_hash_table_insert (priv->request_types, g_strdup ("data"),
			     GSIZE_TO_POINTER (WEBKIT_TYPE_SOUP_REQUEST_DATA));
	g_hash_table_insert (priv->request_types, g_strdup ("http"),
			     GSIZE_TO_POINTER (WEBKIT_TYPE_SOUP_REQUEST_HTTP));
	g_hash_table_insert (priv->request_types, g_strdup ("https"),
			     GSIZE_TO_POINTER (WEBKIT_TYPE_SOUP_REQUEST_HTTP));
	g_hash_table_insert (priv->request_types, g_strdup ("ftp"),
			     GSIZE_TO_POINTER (WEBKIT_TYPE_SOUP_REQUEST_FILE));
}

WebKitSoupRequester *webkit_soup_requester_new (void)
{
	return (WebKitSoupRequester *)g_object_new (WEBKIT_TYPE_SOUP_REQUESTER, NULL);
}

WebKitSoupRequest *webkit_soup_requester_request (WebKitSoupRequester *requester, const char *uriString, SoupSession *session, GError **error)
{
	SoupURI *uri = NULL;
	WebKitSoupRequest *req;

	uri = soup_uri_new (uriString);
	if (!uri) {
		g_set_error (error, WEBKIT_SOUP_ERROR, WEBKIT_SOUP_ERROR_BAD_URI,
			     _ ("Could not parse URI '%s'"), uriString);
		return 0;
	}

	req = webkit_soup_requester_request_uri (requester, uri, session, error);
	soup_uri_free (uri);
	return req;
}

WebKitSoupRequest *webkit_soup_requester_request_uri (WebKitSoupRequester *requester, SoupURI *uri, SoupSession *session, GError **error)
{
	GType requestType;

	g_return_val_if_fail (WEBKIT_IS_SOUP_REQUESTER (requester), 0);

	init_request_types (requester->priv);
	requestType = (GType)GPOINTER_TO_SIZE (g_hash_table_lookup (requester->priv->request_types, uri->scheme));
	if (!requestType) {
		g_set_error (error, WEBKIT_SOUP_ERROR, WEBKIT_SOUP_ERROR_UNSUPPORTED_URI_SCHEME,
			     _ ("Unsupported URI scheme '%s'"), uri->scheme);
		return 0;
	}

	if (g_type_is_a (requestType, G_TYPE_INITABLE)) {
		return (WebKitSoupRequest *)g_initable_new (requestType, 0, error,
							    "uri", uri,
							    "session", session,
							    NULL);
	} else {
		return (WebKitSoupRequest *)g_object_new (requestType,
							  "uri", uri,
							  "session", session,
							  NULL);
	}
}

/* RFC 2396, 3.1 */
static gboolean
soup_scheme_is_valid (const char *scheme)
{
	if (scheme == NULL ||
	    !g_ascii_isalpha (*scheme))
		return FALSE;

	scheme++;
	while (*scheme) {
		if (!g_ascii_isalpha (*scheme) &&
		    !g_ascii_isdigit (*scheme) &&
		    *scheme != '+' &&
		    *scheme != '-' &&
		    *scheme != '.')
			return FALSE;
		scheme++;
	}
	return TRUE;
}

void
webkit_soup_requester_add_protocol (WebKitSoupRequester *requester,
				    const char  *scheme,
				    GType request_type)
{
	g_return_if_fail (WEBKIT_IS_SOUP_REQUESTER (requester));
	g_return_if_fail (soup_scheme_is_valid (scheme));

	init_request_types (requester->priv);
	g_hash_table_insert (requester->priv->request_types, g_strdup (scheme),
			     GSIZE_TO_POINTER (request_type));
}

void
webkit_soup_requester_remove_protocol (WebKitSoupRequester *requester,
				       const char  *scheme)
{
	g_return_if_fail (WEBKIT_IS_SOUP_REQUESTER (requester));
	g_return_if_fail (soup_scheme_is_valid (scheme));

	init_request_types (requester->priv);
	g_hash_table_remove (requester->priv->request_types, scheme);
}

GQuark
webkit_soup_error_quark (void)
{
	static GQuark error;
	if (!error)
		error = g_quark_from_static_string ("webkit_soup_error_quark");
	return error;
}
