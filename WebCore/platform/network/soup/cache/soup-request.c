/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-request.c: Protocol-independent streaming request interface
 *
 * Copyright (C) 2009 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "soup-request.h"
#include "soup-requester.h"

/**
 * SECTION:soup-request
 * @short_description: Protocol-independent streaming request interface
 *
 * FIXME
 **/

/**
 * WebKitSoupRequest:
 *
 * FIXME
 *
 * Since: 2.30
 **/

static void webkit_soup_request_initable_interface_init (GInitableIface *initable_interface);

G_DEFINE_TYPE_WITH_CODE (WebKitSoupRequest, webkit_soup_request, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						webkit_soup_request_initable_interface_init))

enum {
	PROP_0,
	PROP_URI,
	PROP_SESSION
};

struct _WebKitSoupRequestPrivate {
	SoupURI *uri;
	SoupSession *session;
};

static void
webkit_soup_request_init (WebKitSoupRequest *request)
{
	request->priv = G_TYPE_INSTANCE_GET_PRIVATE (request, WEBKIT_TYPE_SOUP_REQUEST, WebKitSoupRequestPrivate);
}

static void
webkit_soup_request_finalize (GObject *object)
{
	WebKitSoupRequest *request = WEBKIT_SOUP_REQUEST (object);

	if (request->priv->uri)
		soup_uri_free (request->priv->uri);
	if (request->priv->session)
		g_object_unref (request->priv->session);

	G_OBJECT_CLASS (webkit_soup_request_parent_class)->finalize (object);
}

static void
webkit_soup_request_set_property (GObject      *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
	WebKitSoupRequest *request = WEBKIT_SOUP_REQUEST (object);

	switch (prop_id) {
	case PROP_URI:
		if (request->priv->uri)
			soup_uri_free (request->priv->uri);
		request->priv->uri = g_value_dup_boxed (value);
		break;
	case PROP_SESSION:
		if (request->priv->session)
			g_object_unref (request->priv->session);
		request->priv->session = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
webkit_soup_request_get_property (GObject    *object,
				  guint prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
	WebKitSoupRequest *request = WEBKIT_SOUP_REQUEST (object);

	switch (prop_id) {
	case PROP_URI:
		g_value_set_boxed (value, request->priv->uri);
		break;
	case PROP_SESSION:
		g_value_set_object (value, request->priv->session);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gboolean
webkit_soup_request_initable_init (GInitable     *initable,
				   GCancellable  *cancellable,
				   GError       **error)
{
	WebKitSoupRequest *request = WEBKIT_SOUP_REQUEST (initable);
	gboolean ok;

	if (!request->priv->uri) {
		g_set_error (error, WEBKIT_SOUP_ERROR, WEBKIT_SOUP_ERROR_BAD_URI,
			     _ ("No URI provided"));
		return FALSE;
	}

	ok = WEBKIT_SOUP_REQUEST_GET_CLASS (initable)->
		check_uri (request, request->priv->uri, error);

	if (!ok && error) {
		char *uri_string = soup_uri_to_string (request->priv->uri, FALSE);
		g_set_error (error, WEBKIT_SOUP_ERROR, WEBKIT_SOUP_ERROR_BAD_URI,
			     _ ("Invalid '%s' URI: %s"),
			     request->priv->uri->scheme,
			     uri_string);
		g_free (uri_string);
	}

	return ok;
}

static gboolean
webkit_soup_request_default_check_uri (WebKitSoupRequest  *request,
				       SoupURI      *uri,
				       GError      **error)
{
	return TRUE;
}

/* Default implementation: assume the sync implementation doesn't block */
static void
webkit_soup_request_default_send_async (WebKitSoupRequest          *request,
					GCancellable         *cancellable,
					GAsyncReadyCallback callback,
					gpointer user_data)
{
	GSimpleAsyncResult *simple;

	simple = g_simple_async_result_new (G_OBJECT (request),
					    callback, user_data,
					    webkit_soup_request_default_send_async);
	g_simple_async_result_complete_in_idle (simple);
	g_object_unref (simple);
}

static GInputStream *
webkit_soup_request_default_send_finish (WebKitSoupRequest          *request,
					 GAsyncResult         *result,
					 GError              **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (request), webkit_soup_request_default_send_async), NULL);

	return webkit_soup_request_send (request, NULL, error);
}

GInputStream *
webkit_soup_request_send (WebKitSoupRequest          *request,
			  GCancellable         *cancellable,
			  GError              **error)
{
	return WEBKIT_SOUP_REQUEST_GET_CLASS (request)->
		send (request, cancellable, error);
}

void
webkit_soup_request_send_async (WebKitSoupRequest          *request,
				GCancellable         *cancellable,
				GAsyncReadyCallback callback,
				gpointer user_data)
{
	WEBKIT_SOUP_REQUEST_GET_CLASS (request)->
		send_async (request, cancellable, callback, user_data);
}

GInputStream *
webkit_soup_request_send_finish (WebKitSoupRequest          *request,
				 GAsyncResult         *result,
				 GError              **error)
{
	return WEBKIT_SOUP_REQUEST_GET_CLASS (request)->
		send_finish (request, result, error);
}

static void
webkit_soup_request_class_init (WebKitSoupRequestClass *request_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (request_class);

	g_type_class_add_private (request_class, sizeof (WebKitSoupRequestPrivate));

	request_class->check_uri = webkit_soup_request_default_check_uri;
	request_class->send_async = webkit_soup_request_default_send_async;
	request_class->send_finish = webkit_soup_request_default_send_finish;

	object_class->finalize = webkit_soup_request_finalize;
	object_class->set_property = webkit_soup_request_set_property;
	object_class->get_property = webkit_soup_request_get_property;

	g_object_class_install_property (
		 object_class, PROP_URI,
		 g_param_spec_boxed (WEBKIT_SOUP_REQUEST_URI,
				     "URI",
				     "The request URI",
				     SOUP_TYPE_URI,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (
		 object_class, PROP_SESSION,
		 g_param_spec_object (WEBKIT_SOUP_REQUEST_SESSION,
				      "Session",
				      "The request's session",
				      SOUP_TYPE_SESSION,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
webkit_soup_request_initable_interface_init (GInitableIface *initable_interface)
{
	initable_interface->init = webkit_soup_request_initable_init;
}

SoupURI *
webkit_soup_request_get_uri (WebKitSoupRequest *request)
{
	return request->priv->uri;
}

SoupSession *
webkit_soup_request_get_session (WebKitSoupRequest *request)
{
	return request->priv->session;
}

goffset
webkit_soup_request_get_content_length (WebKitSoupRequest *request)
{
	return WEBKIT_SOUP_REQUEST_GET_CLASS (request)->get_content_length (request);
}

const char *
webkit_soup_request_get_content_type (WebKitSoupRequest  *request)
{
	return WEBKIT_SOUP_REQUEST_GET_CLASS (request)->get_content_type (request);
}

#define XDIGIT(c) ((c) <= '9' ? (c) - '0' : ((c) & 0x4F) - 'A' + 10)
#define HEXCHAR(s) ((XDIGIT (s[1]) << 4) + XDIGIT (s[2]))

/* Copy&pasted from libsoup's soup-uri.c after applying the patch in
 * https://bugzilla.gnome.org/show_bug.cgi?id=630540. We need this
 * instead of soup_uri_decode() as it incorrectly returns NULL for
 * incorrectly encoded URLs. TODO: remove this when required libsoup
 * version is bumped out to 2.32.1
 */
gchar *
webkit_soup_request_uri_decoded_copy (const char *part, int length)
{
	unsigned char *s, *d;
        char *decoded = g_strndup (part, length);

        s = d = (unsigned char *)decoded;
        do {
                if (*s == '%') {
                        if (!g_ascii_isxdigit (s[1]) ||
                            !g_ascii_isxdigit (s[2])) {
                                *d++ = *s;
                                continue;
                        }
                        *d++ = HEXCHAR (s);
                        s += 2;
                } else
                        *d++ = *s;
        } while (*s++);

        return decoded;
}
