/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-request-file.c: file: URI request object
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

#include "soup-request-file.h"
#include "soup-directory-input-stream.h"
#include "soup-requester.h"
#include <glib/gi18n.h>

G_DEFINE_TYPE (WebKitSoupRequestFile, webkit_soup_request_file, WEBKIT_TYPE_SOUP_REQUEST)

struct _WebKitSoupRequestFilePrivate {
	GFile *gfile;

	char *mime_type;
	goffset size;
};

GFile *
webkit_soup_request_file_get_file (WebKitSoupRequestFile *file)
{
	return g_object_ref (file->priv->gfile);
}

static void
webkit_soup_request_file_init (WebKitSoupRequestFile *file)
{
	file->priv = G_TYPE_INSTANCE_GET_PRIVATE (file, WEBKIT_TYPE_SOUP_REQUEST_FILE, WebKitSoupRequestFilePrivate);

	file->priv->size = -1;
}

static void
webkit_soup_request_file_finalize (GObject *object)
{
	WebKitSoupRequestFile *file = WEBKIT_SOUP_REQUEST_FILE (object);

	if (file->priv->gfile)
		g_object_unref (file->priv->gfile);
	g_free (file->priv->mime_type);

	G_OBJECT_CLASS (webkit_soup_request_file_parent_class)->finalize (object);
}

static gboolean
webkit_soup_request_file_check_uri (WebKitSoupRequest  *request,
				    SoupURI      *uri,
				    GError      **error)
{
	/* "file:/foo" is not valid */
	if (!uri->host)
		return FALSE;

	/* but it must be "file:///..." or "file://localhost/..." */
	if (uri->scheme == SOUP_URI_SCHEME_FILE &&
	    *uri->host &&
	    g_ascii_strcasecmp (uri->host, "localhost") != 0)
		return FALSE;

	return TRUE;
}

static void
webkit_soup_request_file_ftp_main_loop_quit (GObject      *object,
					     GAsyncResult *result,
					     gpointer loop)
{
	g_main_loop_quit (loop);
}

/* This is a somewhat hacky way to get FTP to almost work. The proper way to
 * get FTP to _really_ work involves hacking GIO to have APIs to handle
 * canoncial URLs.
 */
static GFile *
webkit_soup_request_file_ensure_file_ftp (SoupURI       *uri,
					  GCancellable  *cancellable,
					  GError       **error)
{
	SoupURI *host;
	char *s;
	GFile *file, *result;
	GMount *mount;

	host = soup_uri_copy_host (uri);
	s = soup_uri_to_string (host, FALSE);
	file = g_file_new_for_uri (s);
	soup_uri_free (host);
	g_free (s);

	mount = g_file_find_enclosing_mount (file, cancellable, error);
	if (mount == NULL && g_file_supports_thread_contexts (file)) {
		GMainContext *context = g_main_context_new ();
		GMainLoop *loop = g_main_loop_new (context, FALSE);

		g_clear_error (error);
		g_main_context_push_thread_default (context);
		g_file_mount_enclosing_volume (file,
					       G_MOUNT_MOUNT_NONE,
					       NULL, /* FIXME! */
					       cancellable,
					       webkit_soup_request_file_ftp_main_loop_quit,
					       loop);
		g_main_loop_run (loop);
		g_main_context_pop_thread_default (context);
		g_main_loop_unref (loop);
		g_main_context_unref (context);
		mount = g_file_find_enclosing_mount (file, cancellable, error);
	}
	if (mount == NULL)
		return NULL;
	g_object_unref (file);

	file = g_mount_get_default_location (mount);
	g_object_unref (mount);

	s = g_strdup (uri->path);
	if (strchr (s, ';'))
		*strchr (s, ';') = 0;

	result = g_file_resolve_relative_path (file, s);
	g_free (s);
	g_object_unref (file);

	return result;
}

static gboolean
webkit_soup_request_file_ensure_file (WebKitSoupRequestFile  *file,
				      GCancellable     *cancellable,
				      GError          **error)
{
	SoupURI *uri;

	if (file->priv->gfile)
		return TRUE;

	uri = webkit_soup_request_get_uri (WEBKIT_SOUP_REQUEST (file));
	if (uri->scheme == SOUP_URI_SCHEME_FILE) {
		/* We cannot use soup_uri_decode as it incorrectly
		 * returns NULL for incorrectly encoded URIs (that
		 * could be valid filenames). This will be hopefully
		 * shipped in libsoup 2.32.1 but we want to land this
		 * first. TODO: replace uri_decoded_copy by
		 * soup_uri_decode when the required libsoup version
		 * is bumped out to 2.32.1
		 */
		gchar *decoded_uri = webkit_soup_request_uri_decoded_copy (uri->path, strlen (uri->path));

		if (decoded_uri) {
			/* Do not use new_for_uri() as the decoded URI
			 *  could not be a valid URI
			 */
			file->priv->gfile = g_file_new_for_path (decoded_uri);
			g_free (decoded_uri);
		}

		return TRUE;
	} else if (uri->scheme == SOUP_URI_SCHEME_FTP) {
		file->priv->gfile = webkit_soup_request_file_ensure_file_ftp (uri,
									      cancellable,
									      error);
		return file->priv->gfile != NULL;
	}

	g_set_error (error, WEBKIT_SOUP_ERROR, WEBKIT_SOUP_ERROR_UNSUPPORTED_URI_SCHEME,
		     _ ("Unsupported URI scheme '%s'"), uri->scheme);
	return FALSE;
}

static GInputStream *
webkit_soup_request_file_send (WebKitSoupRequest          *request,
			       GCancellable         *cancellable,
			       GError              **error)
{
	WebKitSoupRequestFile *file = WEBKIT_SOUP_REQUEST_FILE (request);
	GInputStream *stream;
	GError *my_error = NULL;

	if (!webkit_soup_request_file_ensure_file (file, cancellable, error))
		return NULL;

	stream = G_INPUT_STREAM (g_file_read (file->priv->gfile,
					      cancellable, &my_error));
	if (stream == NULL) {
		if (g_error_matches (my_error, G_IO_ERROR, G_IO_ERROR_IS_DIRECTORY)) {
			GFileEnumerator *enumerator;
			g_clear_error (&my_error);
			enumerator = g_file_enumerate_children (file->priv->gfile,
								"*",
								G_FILE_QUERY_INFO_NONE,
								cancellable,
								error);
			if (enumerator) {
				stream = webkit_soup_directory_input_stream_new (enumerator,
										 webkit_soup_request_get_uri (request));
				g_object_unref (enumerator);
				file->priv->mime_type = g_strdup ("text/html");
			}
		} else {
			g_propagate_error (error, my_error);
		}
	} else {
		GFileInfo *info = g_file_query_info (file->priv->gfile,
						     G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
						     G_FILE_ATTRIBUTE_STANDARD_SIZE,
						     0, cancellable, NULL);
		if (info) {
			const char *content_type;
			file->priv->size = g_file_info_get_size (info);
			content_type = g_file_info_get_content_type (info);

			if (content_type)
				file->priv->mime_type = g_content_type_get_mime_type (content_type);
			g_object_unref (info);
		}
	}

	return stream;
}

static void
webkit_soup_request_file_send_async_thread (GSimpleAsyncResult *res,
					    GObject            *object,
					    GCancellable       *cancellable)
{
	GInputStream *stream;
	WebKitSoupRequest *request;
	GError *error = NULL;

	request = WEBKIT_SOUP_REQUEST (object);

	stream = webkit_soup_request_file_send (request, cancellable, &error);

	if (stream == NULL) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
	} else {
		g_simple_async_result_set_op_res_gpointer (res, stream, g_object_unref);
	}
}

static void
webkit_soup_request_file_send_async (WebKitSoupRequest          *request,
				     GCancellable         *cancellable,
				     GAsyncReadyCallback callback,
				     gpointer user_data)
{
	GSimpleAsyncResult *res;

	res = g_simple_async_result_new (G_OBJECT (request), callback, user_data, webkit_soup_request_file_send_async);

	g_simple_async_result_run_in_thread (res, webkit_soup_request_file_send_async_thread, G_PRIORITY_DEFAULT, cancellable);
	g_object_unref (res);
}

static GInputStream *
webkit_soup_request_file_send_finish (WebKitSoupRequest          *request,
				      GAsyncResult         *result,
				      GError              **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);

	g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == webkit_soup_request_file_send_async);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	return g_object_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

static goffset
webkit_soup_request_file_get_content_length (WebKitSoupRequest *request)
{
	WebKitSoupRequestFile *file = WEBKIT_SOUP_REQUEST_FILE (request);

	return file->priv->size;
}

static const char *
webkit_soup_request_file_get_content_type (WebKitSoupRequest *request)
{
	WebKitSoupRequestFile *file = WEBKIT_SOUP_REQUEST_FILE (request);

	if (!file->priv->mime_type)
		return "application/octet-stream";

	return file->priv->mime_type;
}

static void
webkit_soup_request_file_class_init (WebKitSoupRequestFileClass *request_file_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (request_file_class);
	WebKitSoupRequestClass *request_class =
		WEBKIT_SOUP_REQUEST_CLASS (request_file_class);

	g_type_class_add_private (request_file_class, sizeof (WebKitSoupRequestFilePrivate));

	object_class->finalize = webkit_soup_request_file_finalize;

	request_class->check_uri = webkit_soup_request_file_check_uri;
	request_class->send = webkit_soup_request_file_send;
	request_class->send_async = webkit_soup_request_file_send_async;
	request_class->send_finish = webkit_soup_request_file_send_finish;
	request_class->get_content_length = webkit_soup_request_file_get_content_length;
	request_class->get_content_type = webkit_soup_request_file_get_content_type;
}
