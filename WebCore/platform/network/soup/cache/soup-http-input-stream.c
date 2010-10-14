/* soup-input-stream.c, based on gsocketinputstream.c
 *
 * Copyright (C) 2006-2007 Red Hat, Inc.
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

#include <config.h>

#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include <libsoup/soup.h>

#include "soup-http-input-stream.h"

static void webkit_soup_http_input_stream_seekable_iface_init (GSeekableIface *seekable_iface);

G_DEFINE_TYPE_WITH_CODE (WebKitSoupHTTPInputStream, webkit_soup_http_input_stream, G_TYPE_INPUT_STREAM,
			 G_IMPLEMENT_INTERFACE (G_TYPE_SEEKABLE,
						webkit_soup_http_input_stream_seekable_iface_init))

typedef void (*WebKitSoupHTTPInputStreamCallback)(GInputStream *);

typedef struct {
	SoupSession *session;
	GMainContext *async_context;
	SoupMessage *msg;
	gboolean got_headers, finished;
	goffset offset;

	GCancellable *cancellable;
	GSource *cancel_watch;
	WebKitSoupHTTPInputStreamCallback got_headers_cb;
	WebKitSoupHTTPInputStreamCallback got_chunk_cb;
	WebKitSoupHTTPInputStreamCallback finished_cb;
	WebKitSoupHTTPInputStreamCallback cancelled_cb;

	guchar *leftover_buffer;
	gsize leftover_bufsize, leftover_offset;

	guchar *caller_buffer;
	gsize caller_bufsize, caller_nread;
	GAsyncReadyCallback outstanding_callback;
	GSimpleAsyncResult *result;
} WebKitSoupHTTPInputStreamPrivate;
#define WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), WEBKIT_TYPE_SOUP_HTTP_INPUT_STREAM, WebKitSoupHTTPInputStreamPrivate))


static gssize   webkit_soup_http_input_stream_read (GInputStream         *stream,
						    void                 *buffer,
						    gsize count,
						    GCancellable         *cancellable,
						    GError              **error);
static gboolean webkit_soup_http_input_stream_close (GInputStream         *stream,
						     GCancellable         *cancellable,
						     GError              **error);
static void     webkit_soup_http_input_stream_read_async (GInputStream         *stream,
							  void                 *buffer,
							  gsize count,
							  int io_priority,
							  GCancellable         *cancellable,
							  GAsyncReadyCallback callback,
							  gpointer data);
static gssize   webkit_soup_http_input_stream_read_finish (GInputStream         *stream,
							   GAsyncResult         *result,
							   GError              **error);
static void     webkit_soup_http_input_stream_close_async (GInputStream         *stream,
							   int io_priority,
							   GCancellable         *cancellable,
							   GAsyncReadyCallback callback,
							   gpointer data);
static gboolean webkit_soup_http_input_stream_close_finish (GInputStream         *stream,
							    GAsyncResult         *result,
							    GError              **error);

static goffset  webkit_soup_http_input_stream_tell (GSeekable            *seekable);

static gboolean webkit_soup_http_input_stream_can_seek (GSeekable            *seekable);
static gboolean webkit_soup_http_input_stream_seek (GSeekable            *seekable,
						    goffset offset,
						    GSeekType type,
						    GCancellable         *cancellable,
						    GError              **error);

static gboolean webkit_soup_http_input_stream_can_truncate (GSeekable            *seekable);
static gboolean webkit_soup_http_input_stream_truncate (GSeekable            *seekable,
							goffset offset,
							GCancellable         *cancellable,
							GError              **error);

static void webkit_soup_http_input_stream_got_headers (SoupMessage *msg, gpointer stream);
static void webkit_soup_http_input_stream_got_chunk (SoupMessage *msg, SoupBuffer *chunk, gpointer stream);
static void webkit_soup_http_input_stream_finished (SoupMessage *msg, gpointer stream);

static void
webkit_soup_http_input_stream_finalize (GObject *object)
{
	WebKitSoupHTTPInputStream *stream = WEBKIT_SOUP_HTTP_INPUT_STREAM (object);
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);

	g_object_unref (priv->session);

	g_signal_handlers_disconnect_by_func (priv->msg, G_CALLBACK (webkit_soup_http_input_stream_got_headers), stream);
	g_signal_handlers_disconnect_by_func (priv->msg, G_CALLBACK (webkit_soup_http_input_stream_got_chunk), stream);
	g_signal_handlers_disconnect_by_func (priv->msg, G_CALLBACK (webkit_soup_http_input_stream_finished), stream);
	g_object_unref (priv->msg);
	g_free (priv->leftover_buffer);

	if (G_OBJECT_CLASS (webkit_soup_http_input_stream_parent_class)->finalize)
		(*G_OBJECT_CLASS (webkit_soup_http_input_stream_parent_class)->finalize)(object);
}

static void
webkit_soup_http_input_stream_class_init (WebKitSoupHTTPInputStreamClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GInputStreamClass *stream_class = G_INPUT_STREAM_CLASS (klass);

	g_type_class_add_private (klass, sizeof (WebKitSoupHTTPInputStreamPrivate));

	gobject_class->finalize = webkit_soup_http_input_stream_finalize;

	stream_class->read_fn = webkit_soup_http_input_stream_read;
	stream_class->close_fn = webkit_soup_http_input_stream_close;
	stream_class->read_async = webkit_soup_http_input_stream_read_async;
	stream_class->read_finish = webkit_soup_http_input_stream_read_finish;
	stream_class->close_async = webkit_soup_http_input_stream_close_async;
	stream_class->close_finish = webkit_soup_http_input_stream_close_finish;
}

static void
webkit_soup_http_input_stream_seekable_iface_init (GSeekableIface *seekable_iface)
{
	seekable_iface->tell = webkit_soup_http_input_stream_tell;
	seekable_iface->can_seek = webkit_soup_http_input_stream_can_seek;
	seekable_iface->seek = webkit_soup_http_input_stream_seek;
	seekable_iface->can_truncate = webkit_soup_http_input_stream_can_truncate;
	seekable_iface->truncate_fn = webkit_soup_http_input_stream_truncate;
}

static void
webkit_soup_http_input_stream_init (WebKitSoupHTTPInputStream *stream)
{
	;
}

static void
webkit_soup_http_input_stream_queue_message (WebKitSoupHTTPInputStream *stream)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);

	priv->got_headers = priv->finished = FALSE;

	/* Add an extra ref since soup_session_queue_message steals one */
	g_object_ref (priv->msg);
	soup_session_queue_message (priv->session, priv->msg, NULL, NULL);
}

/**
 * webkit_soup_http_input_stream_new:
 * @session: the #SoupSession to use
 * @msg: the #SoupMessage whose response will be streamed
 *
 * Prepares to send @msg over @session, and returns a #GInputStream
 * that can be used to read the response.
 *
 * @msg may not be sent until the first read call; if you need to look
 * at the status code or response headers before reading the body, you
 * can use webkit_soup_http_input_stream_send() or webkit_soup_http_input_stream_send_async()
 * to force the message to be sent and the response headers read.
 *
 * If @msg gets a non-2xx result, the first read (or send) will return
 * an error with type %WEBKIT_SOUP_HTTP_INPUT_STREAM_HTTP_ERROR.
 *
 * Internally, #WebKitSoupHTTPInputStream is implemented using asynchronous I/O,
 * so if you are using the synchronous API (eg,
 * g_input_stream_read()), you should create a new #GMainContext and
 * set it as the %SOUP_SESSION_ASYNC_CONTEXT property on @session. (If
 * you don't, then synchronous #GInputStream calls will cause the main
 * loop to be run recursively.) The async #GInputStream API works fine
 * with %SOUP_SESSION_ASYNC_CONTEXT either set or unset.
 *
 * Returns: a new #GInputStream.
 **/
WebKitSoupHTTPInputStream *
webkit_soup_http_input_stream_new (SoupSession *session, SoupMessage *msg)
{
	WebKitSoupHTTPInputStream *stream;
	WebKitSoupHTTPInputStreamPrivate *priv;

	g_return_val_if_fail (SOUP_IS_MESSAGE (msg), NULL);

	stream = g_object_new (WEBKIT_TYPE_SOUP_HTTP_INPUT_STREAM, NULL);
	priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);

	priv->session = g_object_ref (session);
	priv->async_context = soup_session_get_async_context (session);
	priv->msg = g_object_ref (msg);

	g_signal_connect (msg, "got_headers",
			  G_CALLBACK (webkit_soup_http_input_stream_got_headers), stream);
	g_signal_connect (msg, "got_chunk",
			  G_CALLBACK (webkit_soup_http_input_stream_got_chunk), stream);
	g_signal_connect (msg, "finished",
			  G_CALLBACK (webkit_soup_http_input_stream_finished), stream);

	webkit_soup_http_input_stream_queue_message (stream);
	return stream;
}

static void
webkit_soup_http_input_stream_got_headers (SoupMessage *msg, gpointer stream)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);

	/* If the status is unsuccessful, we just ignore the signal and let
	 * libsoup keep going (eventually either it will requeue the request
	 * (after handling authentication/redirection), or else the
	 * "finished" handler will run).
	 */
	if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code))
		return;

	priv->got_headers = TRUE;
	if (!priv->caller_buffer) {
		/* Not ready to read the body yet */
		soup_session_pause_message (priv->session, msg);
	}

	if (priv->got_headers_cb)
		priv->got_headers_cb (stream);
}

static void
webkit_soup_http_input_stream_got_chunk (SoupMessage *msg, SoupBuffer *chunk_buffer,
					 gpointer stream)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);
	const gchar *chunk = chunk_buffer->data;
	gsize chunk_size = chunk_buffer->length;

	/* We only pay attention to the chunk if it's part of a successful
	 * response.
	 */
	if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code))
		return;

	/* Sanity check */
	if (priv->caller_bufsize == 0 || priv->leftover_bufsize != 0)
		g_warning ("webkit_soup_http_input_stream_got_chunk called again before previous chunk was processed");

	/* Copy what we can into priv->caller_buffer */
	if (priv->caller_bufsize - priv->caller_nread > 0) {
		gsize nread = MIN (chunk_size, priv->caller_bufsize - priv->caller_nread);

		memcpy (priv->caller_buffer + priv->caller_nread, chunk, nread);
		priv->caller_nread += nread;
		priv->offset += nread;
		chunk += nread;
		chunk_size -= nread;
	}

	if (chunk_size > 0) {
		/* Copy the rest into priv->leftover_buffer. If
		 * there's already some data there, realloc and
		 * append. Otherwise just copy.
		 */
		if (priv->leftover_bufsize) {
			priv->leftover_buffer = g_realloc (priv->leftover_buffer,
							   priv->leftover_bufsize + chunk_size);
			memcpy (priv->leftover_buffer + priv->leftover_bufsize,
				chunk, chunk_size);
			priv->leftover_bufsize += chunk_size;
		} else {
			priv->leftover_bufsize = chunk_size;
			priv->leftover_buffer = g_memdup (chunk, chunk_size);
			priv->leftover_offset = 0;
		}
	}

	soup_session_pause_message (priv->session, msg);
	if (priv->got_chunk_cb)
		priv->got_chunk_cb (stream);
}

static void
webkit_soup_http_input_stream_finished (SoupMessage *msg, gpointer stream)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);

	priv->finished = TRUE;

	if (priv->finished_cb)
		priv->finished_cb (stream);
}

static gboolean
webkit_soup_http_input_stream_cancelled (GIOChannel *chan, GIOCondition condition,
					 gpointer stream)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);

	priv->cancel_watch = NULL;

	soup_session_pause_message (priv->session, priv->msg);
	if (priv->cancelled_cb)
		priv->cancelled_cb (stream);

	return FALSE;
}

static void
webkit_soup_http_input_stream_prepare_for_io (GInputStream *stream,
					      GCancellable *cancellable,
					      guchar       *buffer,
					      gsize count)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);
	int cancel_fd;

	priv->cancellable = cancellable;
	cancel_fd = g_cancellable_get_fd (cancellable);
	if (cancel_fd != -1) {
		GIOChannel *chan = g_io_channel_unix_new (cancel_fd);
		priv->cancel_watch = soup_add_io_watch (priv->async_context, chan,
							G_IO_IN | G_IO_ERR | G_IO_HUP,
							webkit_soup_http_input_stream_cancelled,
							stream);
		g_io_channel_unref (chan);
	}

	priv->caller_buffer = buffer;
	priv->caller_bufsize = count;
	priv->caller_nread = 0;

	if (priv->got_headers)
		soup_session_unpause_message (priv->session, priv->msg);
}

static void
webkit_soup_http_input_stream_done_io (GInputStream *stream)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);

	if (priv->cancel_watch) {
		g_source_destroy (priv->cancel_watch);
		priv->cancel_watch = NULL;
		g_cancellable_release_fd (priv->cancellable);
	}
	priv->cancellable = NULL;

	priv->caller_buffer = NULL;
	priv->caller_bufsize = 0;
}

static gboolean
set_error_if_http_failed (SoupMessage *msg, GError **error)
{
	if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
		g_set_error_literal (error, SOUP_HTTP_ERROR,
				     msg->status_code, msg->reason_phrase);
		return TRUE;
	}
	return FALSE;
}

static gsize
read_from_leftover (WebKitSoupHTTPInputStreamPrivate *priv,
		    gpointer buffer, gsize bufsize)
{
	gsize nread;

	if (priv->leftover_bufsize - priv->leftover_offset <= bufsize) {
		nread = priv->leftover_bufsize - priv->leftover_offset;
		memcpy (buffer, priv->leftover_buffer + priv->leftover_offset, nread);

		g_free (priv->leftover_buffer);
		priv->leftover_buffer = NULL;
		priv->leftover_bufsize = priv->leftover_offset = 0;
	} else {
		nread = bufsize;
		memcpy (buffer, priv->leftover_buffer + priv->leftover_offset, nread);
		priv->leftover_offset += nread;
	}

	priv->offset += nread;
	return nread;
}

/* This does the work of webkit_soup_http_input_stream_send(), assuming that the
 * GInputStream pending flag has already been set. It is also used by
 * webkit_soup_http_input_stream_send_async() in some circumstances.
 */
static gboolean
webkit_soup_http_input_stream_send_internal (GInputStream  *stream,
					     GCancellable  *cancellable,
					     GError       **error)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);

	webkit_soup_http_input_stream_prepare_for_io (stream, cancellable, NULL, 0);
	while (!priv->finished && !priv->got_headers &&
	       !g_cancellable_is_cancelled (cancellable))
		g_main_context_iteration (priv->async_context, TRUE);
	webkit_soup_http_input_stream_done_io (stream);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;
	else if (set_error_if_http_failed (priv->msg, error))
		return FALSE;
	return TRUE;
}

static void
send_sync_finished (GInputStream *stream)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);
	GError *error = NULL;

	if (!g_cancellable_set_error_if_cancelled (priv->cancellable, &error))
		set_error_if_http_failed (priv->msg, &error);

	priv->got_headers_cb = NULL;
	priv->finished_cb = NULL;

	/* Wake up the main context iteration */
	g_source_attach (g_idle_source_new (), NULL);
}

/**
 * webkit_soup_http_input_stream_send:
 * @httpstream: a #WebKitSoupHTTPInputStream
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occuring, or %NULL to ignore
 *
 * Synchronously sends the HTTP request associated with @stream, and
 * reads the response headers. Call this after webkit_soup_http_input_stream_new()
 * and before the first g_input_stream_read() if you want to check the
 * HTTP status code before you start reading.
 *
 * Return value: %TRUE if msg has a successful (2xx) status, %FALSE if
 * not.
 **/
gboolean
webkit_soup_http_input_stream_send (WebKitSoupHTTPInputStream  *httpstream,
				    GCancellable         *cancellable,
				    GError              **error)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (httpstream);
	GInputStream *istream = (GInputStream *)httpstream;
	gboolean result;

	g_return_val_if_fail (WEBKIT_IS_SOUP_HTTP_INPUT_STREAM (httpstream), FALSE);

	if (!g_input_stream_set_pending (istream, error))
		return FALSE;

	priv->got_headers_cb = send_sync_finished;
	priv->finished_cb = send_sync_finished;

	result = webkit_soup_http_input_stream_send_internal (istream, cancellable, error);
	g_input_stream_clear_pending (istream);

	return result;
}

static gssize
webkit_soup_http_input_stream_read (GInputStream *stream,
				    void         *buffer,
				    gsize count,
				    GCancellable *cancellable,
				    GError      **error)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);

	if (priv->finished)
		return 0;

	/* If there is data leftover from a previous read, return it. */
	if (priv->leftover_bufsize)
		return read_from_leftover (priv, buffer, count);

	/* No leftover data, accept one chunk from the network */
	webkit_soup_http_input_stream_prepare_for_io (stream, cancellable, buffer, count);
	while (!priv->finished && priv->caller_nread == 0 &&
	       !g_cancellable_is_cancelled (cancellable))
		g_main_context_iteration (priv->async_context, TRUE);
	webkit_soup_http_input_stream_done_io (stream);

	if (priv->caller_nread > 0)
		return priv->caller_nread;

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return -1;
	else if (set_error_if_http_failed (priv->msg, error))
		return -1;
	else
		return 0;
}

static gboolean
webkit_soup_http_input_stream_close (GInputStream *stream,
				     GCancellable *cancellable,
				     GError      **error)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);

	if (!priv->finished)
		soup_session_cancel_message (priv->session, priv->msg, SOUP_STATUS_CANCELLED);

	return TRUE;
}

static void
wrapper_callback (GObject *source_object, GAsyncResult *res,
		  gpointer user_data)
{
	GInputStream *stream = G_INPUT_STREAM (source_object);
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);

	g_input_stream_clear_pending (stream);
	if (priv->outstanding_callback)
		(*priv->outstanding_callback)(source_object, res, user_data);
	priv->outstanding_callback = NULL;
	g_object_unref (stream);
}

static void
send_async_thread (GSimpleAsyncResult *res,
		   GObject *object,
		   GCancellable *cancellable)
{
	GError *error = NULL;
	gboolean success;

	success = webkit_soup_http_input_stream_send_internal (G_INPUT_STREAM (object),
							cancellable, &error);
	g_simple_async_result_set_op_res_gboolean (res, success);
	if (error) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
	}
}

static void
webkit_soup_http_input_stream_send_async_in_thread (GInputStream        *stream,
						    int io_priority,
						    GCancellable        *cancellable,
						    GAsyncReadyCallback callback,
						    gpointer user_data)
{
	GSimpleAsyncResult *res;

	res = g_simple_async_result_new (G_OBJECT (stream), callback, user_data,
					 webkit_soup_http_input_stream_send_async_in_thread);
	g_simple_async_result_run_in_thread (res, send_async_thread,
					     io_priority, cancellable);
	g_object_unref (res);
}

static void
send_async_finished (GInputStream *stream)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);
	GSimpleAsyncResult *result;
	GError *error = NULL;

	if (!g_cancellable_set_error_if_cancelled (priv->cancellable, &error))
		set_error_if_http_failed (priv->msg, &error);

	priv->got_headers_cb = NULL;
	priv->finished_cb = NULL;
	webkit_soup_http_input_stream_done_io (stream);

	result = priv->result;
	priv->result = NULL;

	g_simple_async_result_set_op_res_gboolean (result, error == NULL);
	if (error) {
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
	}
	g_simple_async_result_complete (result);
}

static void
webkit_soup_http_input_stream_send_async_internal (GInputStream        *stream,
						   int io_priority,
						   GCancellable        *cancellable,
						   GAsyncReadyCallback callback,
						   gpointer user_data)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);

	g_object_ref (stream);
	priv->outstanding_callback = callback;

	/* If the session uses the default GMainContext, then we can do
	 * async I/O directly. But if it has its own main context, it's
	 * easier to just run it in another thread.
	 */
	if (soup_session_get_async_context (priv->session)) {
		webkit_soup_http_input_stream_send_async_in_thread (stream, io_priority, cancellable,
							     wrapper_callback, user_data);
		return;
	}

	priv->got_headers_cb = send_async_finished;
	priv->finished_cb = send_async_finished;

	webkit_soup_http_input_stream_prepare_for_io (stream, cancellable, NULL, 0);
	priv->result = g_simple_async_result_new (G_OBJECT (stream),
						  wrapper_callback, user_data,
						  webkit_soup_http_input_stream_send_async);
}

/**
 * webkit_soup_http_input_stream_send_async:
 * @httpstream: a #WebKitSoupHTTPInputStream
 * @io_priority: the io priority of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: callback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously sends the HTTP request associated with @stream, and
 * reads the response headers. Call this after webkit_soup_http_input_stream_new()
 * and before the first g_input_stream_read_async() if you want to
 * check the HTTP status code before you start reading.
 **/
void
webkit_soup_http_input_stream_send_async (WebKitSoupHTTPInputStream *httpstream,
					  int io_priority,
					  GCancellable        *cancellable,
					  GAsyncReadyCallback callback,
					  gpointer user_data)
{
	GInputStream *istream = (GInputStream *)httpstream;
	GError *error = NULL;

	g_return_if_fail (WEBKIT_IS_SOUP_HTTP_INPUT_STREAM (httpstream));

	if (!g_input_stream_set_pending (istream, &error)) {
		g_simple_async_report_gerror_in_idle (G_OBJECT (httpstream),
						      callback,
						      user_data,
						      error);
		g_error_free (error);
		return;
	}
	webkit_soup_http_input_stream_send_async_internal (istream, io_priority, cancellable,
						    callback, user_data);
}

/**
 * webkit_soup_http_input_stream_send_finish:
 * @httpstream: a #WebKitSoupHTTPInputStream
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occuring, or %NULL to
 * ignore.
 *
 * Finishes a webkit_soup_http_input_stream_send_async() operation.
 *
 * Return value: %TRUE if the message was sent successfully and
 * received a successful status code, %FALSE if not.
 **/
gboolean
webkit_soup_http_input_stream_send_finish (WebKitSoupHTTPInputStream  *httpstream,
					   GAsyncResult         *result,
					   GError              **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
	simple = G_SIMPLE_ASYNC_RESULT (result);

	g_return_val_if_fail (g_simple_async_result_get_source_tag (simple) == webkit_soup_http_input_stream_send_async, FALSE);

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	return g_simple_async_result_get_op_res_gboolean (simple);
}

static void
read_async_done (GInputStream *stream)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);
	GSimpleAsyncResult *result;
	GError *error = NULL;

	result = priv->result;
	priv->result = NULL;

	if (g_cancellable_set_error_if_cancelled (priv->cancellable, &error) ||
	    set_error_if_http_failed (priv->msg, &error)) {
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
	} else
		g_simple_async_result_set_op_res_gssize (result, priv->caller_nread);

	priv->got_chunk_cb = NULL;
	priv->finished_cb = NULL;
	priv->cancelled_cb = NULL;
	webkit_soup_http_input_stream_done_io (stream);

	g_simple_async_result_complete (result);
	g_object_unref (result);
}

static void
webkit_soup_http_input_stream_read_async (GInputStream        *stream,
					  void                *buffer,
					  gsize count,
					  int io_priority,
					  GCancellable        *cancellable,
					  GAsyncReadyCallback callback,
					  gpointer user_data)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (stream);
	GSimpleAsyncResult *result;

	/* If the session uses the default GMainContext, then we can do
	 * async I/O directly. But if it has its own main context, we fall
	 * back to the async-via-sync-in-another-thread implementation.
	 */
	if (soup_session_get_async_context (priv->session)) {
		G_INPUT_STREAM_CLASS (webkit_soup_http_input_stream_parent_class)->
		read_async (stream, buffer, count, io_priority,
			    cancellable, callback, user_data);
		return;
	}

	result = g_simple_async_result_new (G_OBJECT (stream),
					    callback, user_data,
					    webkit_soup_http_input_stream_read_async);

	if (priv->finished) {
		g_simple_async_result_set_op_res_gssize (result, 0);
		g_simple_async_result_complete_in_idle (result);
		g_object_unref (result);
		return;
	}

	if (priv->leftover_bufsize) {
		gsize nread = read_from_leftover (priv, buffer, count);
		g_simple_async_result_set_op_res_gssize (result, nread);
		g_simple_async_result_complete_in_idle (result);
		g_object_unref (result);
		return;
	}

	priv->result = result;

	priv->got_chunk_cb = read_async_done;
	priv->finished_cb = read_async_done;
	priv->cancelled_cb = read_async_done;
	webkit_soup_http_input_stream_prepare_for_io (stream, cancellable, buffer, count);
}

static gssize
webkit_soup_http_input_stream_read_finish (GInputStream  *stream,
					   GAsyncResult  *result,
					   GError       **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), -1);
	simple = G_SIMPLE_ASYNC_RESULT (result);
	g_return_val_if_fail (g_simple_async_result_get_source_tag (simple) == webkit_soup_http_input_stream_read_async, -1);

	return g_simple_async_result_get_op_res_gssize (simple);
}

static void
webkit_soup_http_input_stream_close_async (GInputStream       *stream,
					   int io_priority,
					   GCancellable       *cancellable,
					   GAsyncReadyCallback callback,
					   gpointer user_data)
{
	GSimpleAsyncResult *result;
	gboolean success;
	GError *error = NULL;

	result = g_simple_async_result_new (G_OBJECT (stream),
					    callback, user_data,
					    webkit_soup_http_input_stream_close_async);
	success = webkit_soup_http_input_stream_close (stream, cancellable, &error);
	g_simple_async_result_set_op_res_gboolean (result, success);
	if (error) {
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
	}

	g_simple_async_result_complete_in_idle (result);
	g_object_unref (result);
}

static gboolean
webkit_soup_http_input_stream_close_finish (GInputStream  *stream,
					    GAsyncResult  *result,
					    GError       **error)
{
	/* Failures handled in generic close_finish code */
	return TRUE;
}

static goffset
webkit_soup_http_input_stream_tell (GSeekable *seekable)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (seekable);

	return priv->offset;
}

static gboolean
webkit_soup_http_input_stream_can_seek (GSeekable *seekable)
{
	return TRUE;
}

extern void soup_message_io_cleanup (SoupMessage *msg);

static gboolean
webkit_soup_http_input_stream_seek (GSeekable     *seekable,
				    goffset offset,
				    GSeekType type,
				    GCancellable  *cancellable,
				    GError       **error)
{
	GInputStream *stream = G_INPUT_STREAM (seekable);
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (seekable);
	char *range;

	if (type == G_SEEK_END) {
		/* FIXME: we could send "bytes=-offset", but unless we
		 * know the Content-Length, we wouldn't be able to
		 * answer a tell() properly. We could find the
		 * Content-Length by doing a HEAD...
		 */

		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
				     "G_SEEK_END not currently supported");
		return FALSE;
	}

	if (!g_input_stream_set_pending (stream, error))
		return FALSE;

	soup_session_cancel_message (priv->session, priv->msg, SOUP_STATUS_CANCELLED);
	soup_message_io_cleanup (priv->msg);

	switch (type) {
		case G_SEEK_CUR:
			offset += priv->offset;
		/* fall through */

		case G_SEEK_SET:
			range = g_strdup_printf ("bytes=%" G_GUINT64_FORMAT "-", (guint64)offset);
			priv->offset = offset;
			break;

		case G_SEEK_END:
			range = NULL; /* keep compilers happy */
			g_return_val_if_reached (FALSE);
			break;

		default:
			g_return_val_if_reached (FALSE);
	}

	soup_message_headers_remove (priv->msg->request_headers, "Range");
	soup_message_headers_append (priv->msg->request_headers, "Range", range);
	g_free (range);

	webkit_soup_http_input_stream_queue_message (WEBKIT_SOUP_HTTP_INPUT_STREAM (stream));

	g_input_stream_clear_pending (stream);
	return TRUE;
}

static gboolean
webkit_soup_http_input_stream_can_truncate (GSeekable *seekable)
{
	return FALSE;
}

static gboolean
webkit_soup_http_input_stream_truncate (GSeekable     *seekable,
					goffset offset,
					GCancellable  *cancellable,
					GError       **error)
{
	g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
			     "Truncate not allowed on input stream");
	return FALSE;
}

SoupMessage *
webkit_soup_http_input_stream_get_message (WebKitSoupHTTPInputStream *httpstream)
{
	WebKitSoupHTTPInputStreamPrivate *priv = WEBKIT_SOUP_HTTP_INPUT_STREAM_GET_PRIVATE (httpstream);
	return priv->msg ? g_object_ref (priv->msg) : NULL;
}
