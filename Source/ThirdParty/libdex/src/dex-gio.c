/*
 * dex-gio.c
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "dex-async-pair-private.h"
#include "dex-future-private.h"
#include "dex-future-set.h"
#include "dex-gio.h"
#include "dex-promise.h"

typedef struct _DexFileInfoList DexFileInfoList;

static DexFileInfoList *
dex_file_info_list_copy (DexFileInfoList *list)
{
  return (DexFileInfoList *)g_list_copy_deep ((GList *)list, (GCopyFunc)g_object_ref, NULL);
}

static void
dex_file_info_list_free (DexFileInfoList *list)
{
  GList *real_list = (GList *)list;
  g_list_free_full (real_list, g_object_unref);
}

typedef struct _DexInetAddressList DexInetAddressList;

G_DEFINE_BOXED_TYPE (DexFileInfoList, dex_file_info_list, dex_file_info_list_copy, dex_file_info_list_free)

static DexInetAddressList *
dex_inet_address_list_copy (DexInetAddressList *list)
{
  return (DexInetAddressList *)g_list_copy_deep ((GList *)list, (GCopyFunc)g_object_ref, NULL);
}

static void
dex_inet_address_list_free (DexInetAddressList *list)
{
  GList *real_list = (GList *)list;
  g_list_free_full (real_list, g_object_unref);
}

G_DEFINE_BOXED_TYPE (DexInetAddressList, dex_inet_address_list, dex_inet_address_list_copy, dex_inet_address_list_free)

static inline DexAsyncPair *
create_async_pair (const char *name)
{
  DexAsyncPair *async_pair;

  async_pair = (DexAsyncPair *)dex_object_create_instance (DEX_TYPE_ASYNC_PAIR);
  dex_future_set_static_name (DEX_FUTURE (async_pair), name);

  return async_pair;
}

static void
dex_input_stream_read_bytes_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;
  GBytes *bytes;

  bytes = g_input_stream_read_bytes_finish (G_INPUT_STREAM (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_boxed (async_pair, G_TYPE_BYTES, bytes);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_input_stream_read_bytes:
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_input_stream_read_bytes (GInputStream *stream,
                             gsize         count,
                             int           priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_input_stream_read_bytes_async (stream,
                                   count,
                                   priority,
                                   async_pair->cancellable,
                                   dex_input_stream_read_bytes_cb,
                                   dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_output_stream_write_bytes_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;
  gssize len;

  len = g_output_stream_write_bytes_finish (G_OUTPUT_STREAM (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_int64 (async_pair, len);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_output_stream_write_bytes:
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_output_stream_write_bytes (GOutputStream *stream,
                               GBytes        *bytes,
                               int            priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_output_stream_write_bytes_async (stream,
                                     bytes,
                                     priority,
                                     async_pair->cancellable,
                                     dex_output_stream_write_bytes_cb,
                                     dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_file_read_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GFileInputStream *stream;
  GError *error = NULL;

  if ((stream = g_file_read_finish (G_FILE (object), result, &error)))
    dex_async_pair_return_object (async_pair, stream);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_file_read:
 * @file: a #GFile
 * @io_priority: IO priority such as %G_PRIORITY_DEFAULT
 *
 * Asynchronously opens a file for reading.
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_file_read (GFile *file,
               int    io_priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_file_read_async (file,
                     io_priority,
                     async_pair->cancellable,
                     dex_file_read_cb,
                     dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_file_replace_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GFileOutputStream *stream;
  GError *error = NULL;

  if ((stream = g_file_replace_finish (G_FILE (object), result, &error)))
    dex_async_pair_return_object (async_pair, stream);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_file_replace:
 * @etag: (nullable)
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_file_replace (GFile            *file,
                  const char       *etag,
                  gboolean          make_backup,
                  GFileCreateFlags  flags,
                  int               priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_file_replace_async (file,
                        etag,
                        make_backup,
                        flags,
                        priority,
                        async_pair->cancellable,
                        dex_file_replace_cb,
                        dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_input_stream_read_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;
  gssize len;

  len = g_input_stream_read_finish (G_INPUT_STREAM (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_int64 (async_pair, len);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_input_stream_read:
 * @buffer: (array length=count) (element-type guint8) (out caller-allocates)
 * @count: (in)
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_input_stream_read (GInputStream *self,
                       gpointer      buffer,
                       gsize         count,
                       int           priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_INPUT_STREAM (self), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_input_stream_read_async (self,
                             buffer,
                             count,
                             priority,
                             async_pair->cancellable,
                             dex_input_stream_read_cb,
                             dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_input_stream_skip_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;
  gssize len;

  len = g_input_stream_skip_finish (G_INPUT_STREAM (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_int64 (async_pair, len);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_input_stream_skip:
 * @count: the number of bytes to skip
 * @io_priority: %G_PRIORITY_DEFAULT or similar priority value
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_input_stream_skip (GInputStream *self,
                       gsize         count,
                       int           io_priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_INPUT_STREAM (self), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_input_stream_skip_async (self,
                             count,
                             io_priority,
                             async_pair->cancellable,
                             dex_input_stream_skip_cb,
                             dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_output_stream_write_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;
  gssize len;

  len = g_output_stream_write_finish (G_OUTPUT_STREAM (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_int64 (async_pair, len);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_output_stream_write:
 * @buffer: (array length=count) (element-type guint8)
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_output_stream_write (GOutputStream *self,
                         gconstpointer  buffer,
                         gsize          count,
                         int            priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (self), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_output_stream_write_async (self,
                               buffer,
                               count,
                               priority,
                               async_pair->cancellable,
                               dex_output_stream_write_cb,
                               dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_output_stream_close_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;

  g_output_stream_close_finish (G_OUTPUT_STREAM (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_boolean (async_pair, TRUE);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_output_stream_close:
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_output_stream_close (GOutputStream *self,
                         int            priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (self), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_output_stream_close_async (self,
                               priority,
                               async_pair->cancellable,
                               dex_output_stream_close_cb,
                               dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_input_stream_close_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;

  g_input_stream_close_finish (G_INPUT_STREAM (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_boolean (async_pair, TRUE);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_input_stream_close:
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_input_stream_close (GInputStream *self,
                        int           priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_INPUT_STREAM (self), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_input_stream_close_async (self,
                              priority,
                              async_pair->cancellable,
                              dex_input_stream_close_cb,
                              dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_output_stream_splice_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;
  gssize len;

  len = g_output_stream_splice_finish (G_OUTPUT_STREAM (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_int64 (async_pair, len);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_output_stream_splice:
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_output_stream_splice (GOutputStream            *output,
                          GInputStream             *input,
                          GOutputStreamSpliceFlags  flags,
                          int                       io_priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), NULL);
  g_return_val_if_fail (G_IS_INPUT_STREAM (input), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_output_stream_splice_async (output,
                                input,
                                flags,
                                io_priority,
                                async_pair->cancellable,
                                dex_output_stream_splice_cb,
                                dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_file_query_info_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;
  GFileInfo *info;

  info = g_file_query_info_finish (G_FILE (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_object (async_pair, info);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_file_query_info:
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_file_query_info (GFile               *file,
                     const char          *attributes,
                     GFileQueryInfoFlags  flags,
                     int                  io_priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_file_query_info_async (file,
                           attributes,
                           flags,
                           io_priority,
                           async_pair->cancellable,
                           dex_file_query_info_cb,
                           dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_file_make_directory_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;

  g_file_make_directory_finish (G_FILE (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_boolean (async_pair, TRUE);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_file_make_directory:
 * @file: a #GFile
 * @io_priority: IO priority such as %G_PRIORITY_DEFAULT
 *
 * Asynchronously creates a directory and returns #DexFuture which
 * can be observed for the result.
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_file_make_directory (GFile *file,
                         int    io_priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_file_make_directory_async (file,
                               io_priority,
                               async_pair->cancellable,
                               dex_file_make_directory_cb,
                               dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_file_enumerate_children_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;
  GFileEnumerator *enumerator;

  enumerator = g_file_enumerate_children_finish (G_FILE (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_object (async_pair, enumerator);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_file_enumerate_children:
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_file_enumerate_children (GFile               *file,
                             const char          *attributes,
                             GFileQueryInfoFlags  flags,
                             int                  io_priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_file_enumerate_children_async (file,
                                   attributes,
                                   flags,
                                   io_priority,
                                   async_pair->cancellable,
                                   dex_file_enumerate_children_cb,
                                   dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_file_enumerator_next_files_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;
  GList *infos;

  infos = g_file_enumerator_next_files_finish (G_FILE_ENUMERATOR (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_boxed (async_pair, DEX_TYPE_FILE_INFO_LIST, infos);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_file_enumerator_next_files:
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_file_enumerator_next_files (GFileEnumerator *file_enumerator,
                                int              num_files,
                                int              io_priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_FILE_ENUMERATOR (file_enumerator), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_file_enumerator_next_files_async (file_enumerator,
                                      num_files,
                                      io_priority,
                                      async_pair->cancellable,
                                      dex_file_enumerator_next_files_cb,
                                      dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_file_copy_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;

  g_file_copy_finish (G_FILE (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_boolean (async_pair, TRUE);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_file_copy:
 * @source: a #GFile
 * @destination: a #GFile
 * @flags: the #GFileCopyFlags
 * @io_priority: IO priority such as %G_PRIORITY_DEFAULT
 *
 * Asynchronously copies a file and returns a #DexFuture which
 * can be observed for the result.
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_file_copy (GFile          *source,
               GFile          *destination,
               GFileCopyFlags  flags,
               int             io_priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_FILE (source), NULL);
  g_return_val_if_fail (G_IS_FILE (destination), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_file_copy_async (source,
                     destination,
                     flags,
                     io_priority,
                     async_pair->cancellable,
                     NULL, NULL, /* TODO: Progress support */
                     dex_file_copy_cb,
                     dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_socket_listener_accept_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GSocketConnection *conn;
  GError *error = NULL;

  conn = g_socket_listener_accept_finish (G_SOCKET_LISTENER (object), result, NULL, &error);

  if (error == NULL)
    dex_async_pair_return_object (async_pair, conn);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_socket_listener_accept:
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_socket_listener_accept (GSocketListener *listener)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_SOCKET_LISTENER (listener), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_socket_listener_accept_async (listener,
                                  async_pair->cancellable,
                                  dex_socket_listener_accept_cb,
                                  dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_socket_client_connect_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GSocketConnection *conn;
  GError *error = NULL;

  conn = g_socket_client_connect_finish (G_SOCKET_CLIENT (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_object (async_pair, conn);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_socket_client_connect:
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_socket_client_connect (GSocketClient      *socket_client,
                           GSocketConnectable *socket_connectable)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_SOCKET_CLIENT (socket_client), NULL);
  g_return_val_if_fail (G_IS_SOCKET_CONNECTABLE (socket_connectable), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_socket_client_connect_async (socket_client,
                                 socket_connectable,
                                 async_pair->cancellable,
                                 dex_socket_client_connect_cb,
                                 dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_io_stream_close_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;

  g_io_stream_close_finish (G_IO_STREAM (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_boolean (async_pair, TRUE);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_io_stream_close:
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_io_stream_close (GIOStream *io_stream,
                     int        io_priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_IO_STREAM (io_stream), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_io_stream_close_async (io_stream,
                           io_priority,
                           async_pair->cancellable,
                           dex_io_stream_close_cb,
                           dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_resolver_lookup_by_name_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;
  GList *list;

  list = g_resolver_lookup_by_name_finish (G_RESOLVER (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_boxed (async_pair, DEX_TYPE_INET_ADDRESS_LIST, list);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_resolver_lookup_by_name:
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_resolver_lookup_by_name (GResolver  *resolver,
                             const char *address)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);
  g_return_val_if_fail (address != NULL, NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_resolver_lookup_by_name_async (resolver,
                                   address,
                                   async_pair->cancellable,
                                   dex_resolver_lookup_by_name_cb,
                                   dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_file_load_contents_bytes_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;
  char *contents = NULL;
  gsize len = 0;

  g_file_load_contents_finish (G_FILE (object), result, &contents, &len, NULL, &error);

  if (error == NULL)
    dex_async_pair_return_boxed (async_pair, G_TYPE_BYTES, g_bytes_new_take (contents, len));
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_file_load_contents_bytes:
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
dex_file_load_contents_bytes (GFile *file)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_file_load_contents_async (file,
                              async_pair->cancellable,
                              dex_file_load_contents_bytes_cb,
                              dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_dbus_connection_send_message_with_reply_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GDBusMessage *message = NULL;
  GError *error = NULL;

  message = g_dbus_connection_send_message_with_reply_finish (G_DBUS_CONNECTION (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_object (async_pair, message);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_dbus_connection_send_message_with_reply:
 * @connection: a #GDBusConnection
 * @message: a #GDBusMessage
 * @flags:  flags for @message
 * @timeout_msec: timeout in milliseconds, or -1 for default, or %G_MAXINT
 *   for no timeout.
 * @out_serial: (out) (optional): a location for the message serial number
 *
 * Wrapper for g_dbus_connection_send_message_with_reply().
 *
 * Returns: (transfer full): a #DexFuture that will resolve to a #GDBusMessage
 *   or reject with failure.
 *
 * Since: 0.4
 */
DexFuture *
dex_dbus_connection_send_message_with_reply (GDBusConnection       *connection,
                                             GDBusMessage          *message,
                                             GDBusSendMessageFlags  flags,
                                             int                    timeout_msec,
                                             guint32               *out_serial)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_dbus_connection_send_message_with_reply (connection,
                                             message,
                                             flags,
                                             timeout_msec,
                                             out_serial,
                                             async_pair->cancellable,
                                             dex_dbus_connection_send_message_with_reply_cb,
                                             dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_dbus_connection_call_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GVariant *reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_variant (async_pair, reply);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_dbus_connection_call:
 * @bus_name: (nullable)
 * @object_path:
 * @interface_name:
 * @method_name:
 * @parameters: (nullable)
 * @reply_type: (nullable)
 * @flags:
 * @timeout_msec:
 *
 * Wrapper for g_dbus_connection_call().
 *
 * Returns: (transfer full): a #DexFuture that resolves to a #GVariant
 *   or rejects with error.
 *
 * Since: 0.4
 */
DexFuture *
dex_dbus_connection_call (GDBusConnection    *connection,
                          const char         *bus_name,
                          const char         *object_path,
                          const char         *interface_name,
                          const char         *method_name,
                          GVariant           *parameters,
                          const GVariantType *reply_type,
                          GDBusCallFlags      flags,
                          int                 timeout_msec)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_dbus_connection_call (connection,
                          bus_name,
                          object_path,
                          interface_name,
                          method_name,
                          parameters,
                          reply_type,
                          flags,
                          timeout_msec,
                          async_pair->cancellable,
                          dex_dbus_connection_call_cb,
                          dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

#ifdef G_OS_UNIX
static void
dex_dbus_connection_call_with_unix_fd_list_cb (GObject      *object,
                                               GAsyncResult *result,
                                               gpointer      user_data)
{
  DexFutureSet *future_set = user_data;
  DexAsyncPair *async_pair;
  DexPromise *promise;
  GUnixFDList *fd_list = NULL;
  GVariant *reply = NULL;
  GError *error = NULL;

  g_assert (G_IS_DBUS_CONNECTION (object));
  g_assert (DEX_IS_FUTURE_SET (future_set));

  async_pair = DEX_ASYNC_PAIR (dex_future_set_get_future_at (future_set, 0));
  promise = DEX_PROMISE (dex_future_set_get_future_at (future_set, 1));

  g_assert (DEX_IS_ASYNC_PAIR (async_pair));
  g_assert (DEX_IS_PROMISE (promise));

  reply = g_dbus_connection_call_with_unix_fd_list_finish (G_DBUS_CONNECTION (object), &fd_list, result, &error);

  g_assert (!fd_list || G_IS_UNIX_FD_LIST (fd_list));
  g_assert (reply != NULL || error != NULL);

  if (error == NULL)
    {
      dex_promise_resolve_object (promise, fd_list);
      dex_async_pair_return_variant (async_pair, reply);
    }
  else
    {
      dex_promise_reject (promise, g_error_copy (error));
      dex_async_pair_return_error (async_pair, error);
    }

  dex_unref (future_set);
}

/**
 * dex_dbus_connection_call_with_unix_fd_list:
 * @bus_name: (nullable)
 * @object_path:
 * @interface_name:
 * @method_name:
 * @parameters: (nullable)
 * @reply_type: (nullable)
 * @flags:
 * @timeout_msec:
 * @fd_list: (nullable): a #GUnixFDList
 *
 * Wrapper for g_dbus_connection_call_with_unix_fd_list().
 *
 * Returns: (transfer full): a #DexFutureSet that resolves to a #GVariant.
 *   The #DexFuture containing the resulting #GUnixFDList can be retrieved
 *   with dex_future_set_get_future_at() with an index of 1.
 *
 * Since: 0.4
 */
DexFuture *
dex_dbus_connection_call_with_unix_fd_list (GDBusConnection    *connection,
                                            const char         *bus_name,
                                            const char         *object_path,
                                            const char         *interface_name,
                                            const char         *method_name,
                                            GVariant           *parameters,
                                            const GVariantType *reply_type,
                                            GDBusCallFlags      flags,
                                            int                 timeout_msec,
                                            GUnixFDList        *fd_list)
{
  DexAsyncPair *async_pair;
  DexPromise *promise;
  DexFuture *ret;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (!fd_list || G_IS_UNIX_FD_LIST (fd_list), NULL);

  /* Will hold our GVariant result */
  async_pair = create_async_pair (G_STRFUNC);

  /* Will hold our GUnixFDList result */
  promise = dex_promise_new ();

  /* Sent to user. Resolving will contain variant. */
  ret = dex_future_all (DEX_FUTURE (async_pair), DEX_FUTURE (promise), NULL);

  g_dbus_connection_call_with_unix_fd_list (connection,
                                            bus_name,
                                            object_path,
                                            interface_name,
                                            method_name,
                                            parameters,
                                            reply_type,
                                            flags,
                                            timeout_msec,
                                            fd_list,
                                            async_pair->cancellable,
                                            dex_dbus_connection_call_with_unix_fd_list_cb,
                                            dex_ref (ret));

  return ret;
}
#endif

static void
dex_bus_get_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GDBusConnection *bus;
  GError *error = NULL;

  bus = g_bus_get_finish (result, &error);

  if (error == NULL)
    dex_async_pair_return_object (async_pair, bus);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_bus_get:
 * @bus_type:
 *
 * Wrapper for g_bus_get().
 *
 * Returns: (transfer full): a #DexFuture that resolves to a #GDBusConnection
 *   or rejects with error.
 *
 * Since: 0.4
 */
DexFuture *
dex_bus_get (GBusType bus_type)
{
  DexAsyncPair *async_pair;

  async_pair = create_async_pair (G_STRFUNC);

  g_bus_get (bus_type,
             async_pair->cancellable,
             dex_bus_get_cb,
             dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_subprocess_wait_check_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GError *error = NULL;

  if (g_subprocess_wait_check_finish (G_SUBPROCESS (object), result, &error))
    dex_async_pair_return_boolean (async_pair, TRUE);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

/**
 * dex_subprocess_wait_check:
 * @subprocess: a #GSubprocess
 *
 * Creates a future that awaits for @subprocess to complete using
 * g_subprocess_wait_check_async().
 *
 * Returns: (transfer full): a #DexFuture that will resolve when @subprocess
 *   exits cleanly or reject upon signal or non-successful exit.
 *
 * Since: 0.4
 */
DexFuture *
dex_subprocess_wait_check (GSubprocess *subprocess)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (G_IS_SUBPROCESS (subprocess), NULL);

  async_pair = create_async_pair (G_STRFUNC);

  g_subprocess_wait_check_async (subprocess,
                                 async_pair->cancellable,
                                 dex_subprocess_wait_check_cb,
                                 dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

static void
dex_file_query_exists_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  DexPromise *promise = user_data;
  GFileInfo *file_info;
  GError *error = NULL;

  file_info = g_file_query_info_finish (G_FILE (object), result, &error);

  if (file_info != NULL)
    dex_promise_resolve_boolean (promise, TRUE);
  else
    dex_promise_reject (promise, g_steal_pointer (&error));

  g_clear_object (&file_info);

  dex_unref (promise);
}

/**
 * dex_file_query_exists:
 * @file: a #GFile
 *
 * Queries to see if @file exists asynchronously.
 *
 * Returns: (transfer full): a #DexFuture that will resolve with %TRUE
 *   if the file exists, otherwise reject with error.
 *
 * Since: 0.6
 */
DexFuture *
dex_file_query_exists (GFile *file)
{
  DexPromise *promise;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  promise = dex_promise_new_cancellable ();
  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_STANDARD_TYPE,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           dex_promise_get_cancellable (promise),
                           dex_file_query_exists_cb,
                           dex_ref (promise));
  return DEX_FUTURE (promise);
}
