/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-request-data.c: data: URI request object
 *
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "soup-request-data.h"

#include "soup-requester.h"
#include <libsoup/soup.h>
#include <glib/gi18n.h>

G_DEFINE_TYPE (WebKitSoupRequestData, webkit_soup_request_data, WEBKIT_TYPE_SOUP_REQUEST)

struct _WebKitSoupRequestDataPrivate {
	gsize content_length;
	char *content_type;
};

static void
webkit_soup_request_data_init (WebKitSoupRequestData *data)
{
	data->priv = G_TYPE_INSTANCE_GET_PRIVATE (data, WEBKIT_TYPE_SOUP_REQUEST_DATA, WebKitSoupRequestDataPrivate);
}

static void
webkit_soup_request_data_finalize (GObject *object)
{
	WebKitSoupRequestData *data = WEBKIT_SOUP_REQUEST_DATA (object);

	g_free (data->priv->content_type);

	G_OBJECT_CLASS (webkit_soup_request_data_parent_class)->finalize (object);
}

static gboolean
webkit_soup_request_data_check_uri (WebKitSoupRequest  *request,
				    SoupURI      *uri,
				    GError      **error)
{
	return uri->host == NULL;
}

static GInputStream *
webkit_soup_request_data_send (WebKitSoupRequest   *request,
			       GCancellable  *cancellable,
			       GError       **error)
{
	WebKitSoupRequestData *data = WEBKIT_SOUP_REQUEST_DATA (request);
	SoupURI *uri = webkit_soup_request_get_uri (request);
	GInputStream *memstream;
	const char *comma, *semi, *start, *end;
	gboolean base64 = FALSE;

	gchar *uristr = soup_uri_to_string (uri, FALSE);
	comma = strchr (uristr, ',');
	if (comma && comma != uristr) {
		/* Deal with MIME type / params */
		semi = memchr (uristr, ';', comma - uristr);
		end = semi ? semi : comma;

		if (semi && !g_ascii_strncasecmp (semi, ";base64", MAX ((size_t) (comma - semi), strlen (";base64"))))
			base64 = TRUE;

		if (end != uristr)
			if (base64)
				data->priv->content_type = g_strndup (uristr, end - uristr);
			else
				data->priv->content_type =
					webkit_soup_request_uri_decoded_copy (uristr, end - uristr);
	}

	memstream = g_memory_input_stream_new ();

	start = comma ? comma + 1 : uristr;

	if (*start) {
		guchar *buf;

		if (base64) {
			int inlen, state = 0;
			guint save = 0;

			inlen = strlen (start);
			buf = g_malloc0 (inlen * 3 / 4 + 3);
			data->priv->content_length =
				g_base64_decode_step (start, inlen, buf,
						      &state, &save);
			if (state != 0) {
				g_free (buf);
				goto fail;
			}
		} else {
			/* Cannot use g_uri_unescape_string nor
			   soup_uri_decode because we don't want to
			   fail for things like "%3E%%3C" -> ">%<" */
			buf = (guchar *)webkit_soup_request_uri_decoded_copy (start, strlen (start));
			if (!buf)
				goto fail;
			data->priv->content_length = strlen ((char *)buf);
		}

		g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (memstream),
						buf, data->priv->content_length,
						g_free);
	}
	g_free (uristr);

	return memstream;

 fail:
	g_free (uristr);
	g_set_error (error, WEBKIT_SOUP_ERROR, WEBKIT_SOUP_ERROR_BAD_URI,
		     _ ("Unable to decode URI: %s"), start);
	g_object_unref (memstream);
	return NULL;
}

static goffset
webkit_soup_request_data_get_content_length (WebKitSoupRequest *request)
{
	WebKitSoupRequestData *data = WEBKIT_SOUP_REQUEST_DATA (request);

	return data->priv->content_length;
}

static const char *
webkit_soup_request_data_get_content_type (WebKitSoupRequest *request)
{
	WebKitSoupRequestData *data = WEBKIT_SOUP_REQUEST_DATA (request);

	return data->priv->content_type;
}

static void
webkit_soup_request_data_class_init (WebKitSoupRequestDataClass *request_data_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (request_data_class);
	WebKitSoupRequestClass *request_class =
		WEBKIT_SOUP_REQUEST_CLASS (request_data_class);

	g_type_class_add_private (request_data_class, sizeof (WebKitSoupRequestDataPrivate));

	object_class->finalize = webkit_soup_request_data_finalize;

	request_class->check_uri = webkit_soup_request_data_check_uri;
	request_class->send = webkit_soup_request_data_send;
	request_class->get_content_length = webkit_soup_request_data_get_content_length;
	request_class->get_content_type = webkit_soup_request_data_get_content_type;
}
